// YES DAW — RT-lane shared-memory ring (ADR-0015): the lock-free, double-buffered audio + Event ring
// that implements the one-Block plugin-hosting handshake between the audio thread and a plugin host child.
//
// This is the FIRST plugin-hosting implementation chunk: a HEADLESS primitive whose control words and
// slot payloads live in real OS shared memory (mmap/shm_open or CreateFileMapping). There is still no
// JUCE, no PluginNode-owned real child process, and no coordinator handle-passing yet; tests can attach a
// second RtLaneRing endpoint to the named region and poll it like the future worker child will.
//
// Pure C++ — no JUCE — so the whole protocol is covered by the RTSan leg (exchangeBlock, the audio-thread
// role, under -fsanitize=realtime: it must never allocate/lock/log/syscall) and the TSan leg (a
// free-running producer/consumer stress test, under -fsanitize=thread: the release/acquire protocol must
// be race-free).
//
// ── The protocol (ADR-0015 "RT lane", made concrete) ────────────────────────────────────────────────
// The region holds, per direction, a DOUBLE buffer of "slots" plus a monotonic sequence counter:
//   * input  (audio -> child): inputSlots_[2]  + control_.inputSeq   (release-stored by the audio thread)
//   * output (child -> audio): outputSlots_[2] + control_.outputSeq  (acquire-loaded  by the audio thread)
// Each slot carries: blockIndex, numFrames, an event count, the per-channel audio, and (input slots) the
// block's serializable Events (ADR-0009). A writer always writes the UNpublished slot, so the reader of
// the currently-published slot is normally untouched.
//
// Inside exchangeBlock() (== PluginNode::process(), the audio thread) for Block N:
//   1. write Block N's input (audio + events) into inputSlots_[N % 2]   (plain relaxed-atomic stores)
//   2. release-store control_.inputSeq = N+1                            (publishes the input)
//   3. acquire-load control_.outputSeq and read Block N-1's already-published output, FAIL-OPEN if it
//      is not ready within the Block (last-good -> silence -> bypass) — all branch-only.
// The audio thread NEVER signals, waits, allocates, logs, does I/O, or syscalls. The child is woken off
// the audio thread; here it just polls inputSeq (ADR-0015 allows poll for this chunk).
//
// ── Why a per-slot seqlock version on top of the double buffer ───────────────────────────────────────
// "Double-buffered + the audio thread never blocks + race-free under ARBITRARY timing" cannot be done
// with two PLAIN slots (the classic lock-free-mailbox result that otherwise forces triple buffering): a
// slow reader can be lapped by the writer. The fix that keeps ADR-0015's pinned mechanism (double buffer
// + input/output sequence counters) is a SEQLOCK: every slot has a version (odd while being written,
// even when stable) and its payload words are RELAXED ATOMICS. A reader copies the slot, then re-checks
// the version; a concurrent lap is therefore well-defined (atomic, not UB) and simply discarded as a
// miss — which is exactly what lets the audio thread fail open without waiting and keeps TSan green.

#pragma once

#include "engine/Node.h"   // Event / EventStream (ADR-0009)
#include "rt/RtHot.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined (_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifdef small
        #undef small
    #endif
#else
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

namespace yesdaw::engine {

// What the audio thread got for this Block's output, in fail-open priority order (ADR-0015 / ADR-0002).
enum class RtLaneOutput : std::uint8_t
{
    Silence  = 0,   // nothing valid to deliver -> zeros
    Fresh    = 1,   // the child's freshly-published output for the expected Block
    LastGood = 2,   // the child was late; the previous good output is re-served
    Bypass   = 3    // repeated misses -> latched bypass/placeholder (zeros + a flag for the coordinator)
};

struct RtLaneConfig
{
    int           channels          = 1;
    int           maxBlockSize      = 512;
    std::uint32_t maxEventsPerBlock = 64;
    // Fail-open ladder thresholds (in consecutive missed Blocks). After lastGoodHoldBlocks misses the
    // reader stops re-serving last-good and emits silence; after bypassAfterMisses it latches bypass.
    std::uint32_t lastGoodHoldBlocks = 2;
    std::uint32_t bypassAfterMisses  = 8;
};

struct RtLaneExchangeResult
{
    RtLaneOutput  source           = RtLaneOutput::Silence;
    std::uint64_t inputBlockIndex  = 0;     // the input Block this call published
    std::uint64_t outputBlockIndex = 0;     // Block index of the delivered output (valid iff source==Fresh)
    std::uint32_t outputNumFrames  = 0;     // frame count of the delivered output (valid iff source==Fresh)
    std::uint32_t outputEventCount = 0;     // events the child reported applying for that output Block
    bool          deliveredFresh   = false; // == (source == Fresh)
};

class RtLaneRing
{
public:
    RtLaneRing() = default;
    ~RtLaneRing() = default;

    RtLaneRing (const RtLaneRing&)            = delete;
    RtLaneRing& operator= (const RtLaneRing&) = delete;
    RtLaneRing (RtLaneRing&&)                 = delete;
    RtLaneRing& operator= (RtLaneRing&&)      = delete;

    // CONTROL THREAD. Allocate + size the whole region for a channel count / maximum Block. The ONLY
    // place the ring allocates (mirrors Node::prepare). Not RT-safe.
    void prepare (const RtLaneConfig& cfg)
    {
        prepareSharedMemory (cfg, makeUniqueSharedMemoryName());
    }

    // CONTROL THREAD. Create a named OS-backed shared-memory region and bind this endpoint to it.
    // A future worker endpoint may attach by name. Not RT-safe.
    void prepareSharedMemory (const RtLaneConfig& cfg, std::string_view sharedMemoryName)
    {
        cfg_       = cfg;
        channels_  = std::max (1, cfg.channels);
        maxBlock_  = std::max (1, cfg.maxBlockSize);
        maxEvents_ = cfg.maxEventsPerBlock;

        audioBase_ = kHdrWords;
        eventBase_ = audioBase_ + static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_);
        totalWords_ = eventBase_ + static_cast<std::size_t> (maxEvents_) * kEventWords;

        const Layout layout = computeLayout (totalWords_);
        region_.create (sharedMemoryName, layout.regionBytes);
        sharedMemoryName_ = std::string (sharedMemoryName);
        bindSharedLayout (layout);
        initialiseSharedLayout (layout);

        prepareEndpointScratch();
        reset();
    }

    // CONTROL THREAD. Attach this endpoint to an existing OS-backed region. Returns false when the name is
    // absent or the header is not an RtLaneRing region; it never fabricates an in-process fallback.
    bool attachSharedMemory (std::string_view sharedMemoryName)
    {
        MappedRegion candidate;
        if (! candidate.open (sharedMemoryName))
            return false;

        if (candidate.size() < sizeof (SharedHeader))
            return false;

        const auto* const header = reinterpret_cast<const SharedHeader*> (candidate.data());
        if (header->magic != kSharedMagic || header->version != kSharedVersion)
            return false;
        if (header->regionBytes != candidate.size())
            return false;

        region_ = std::move (candidate);
        sharedMemoryName_ = std::string (sharedMemoryName);

        cfg_.channels = static_cast<int> (header->channels);
        cfg_.maxBlockSize = static_cast<int> (header->maxBlockSize);
        cfg_.maxEventsPerBlock = header->maxEventsPerBlock;
        cfg_.lastGoodHoldBlocks = header->lastGoodHoldBlocks;
        cfg_.bypassAfterMisses = header->bypassAfterMisses;

        channels_ = std::max (1, cfg_.channels);
        maxBlock_ = std::max (1, cfg_.maxBlockSize);
        maxEvents_ = cfg_.maxEventsPerBlock;
        audioBase_ = header->audioBaseWords;
        eventBase_ = header->eventBaseWords;
        totalWords_ = header->totalSlotWords;

        Layout layout;
        layout.regionBytes = header->regionBytes;
        layout.controlOffset = header->controlOffset;
        for (std::size_t i = 0; i < 2; ++i)
        {
            layout.inputVersionOffsets[i] = header->inputVersionOffsets[i];
            layout.inputWordsOffsets[i] = header->inputWordsOffsets[i];
            layout.outputVersionOffsets[i] = header->outputVersionOffsets[i];
            layout.outputWordsOffsets[i] = header->outputWordsOffsets[i];
        }

        bindSharedLayout (layout);
        prepareEndpointScratch();
        return true;
    }

    [[nodiscard]] bool usesOsSharedMemory() const noexcept { return region_.isMapped(); }
    [[nodiscard]] const std::string& sharedMemoryName() const noexcept { return sharedMemoryName_; }

    [[nodiscard]] static std::string makeUniqueSharedMemoryName()
    {
        static std::atomic<std::uint64_t> counter { 0 };
        const std::uint64_t n = counter.fetch_add (1, std::memory_order_relaxed);

#if defined (_WIN32)
        return "Local\\yesdaw_rt_lane_" + std::to_string (GetCurrentProcessId()) + "_" + std::to_string (n);
#else
        return "/yesdaw_rt_lane_" + std::to_string (static_cast<long long> (::getpid())) + "_" + std::to_string (n);
#endif
    }

    // CONTROL THREAD (or between test runs; both endpoints stopped). Clear the protocol back to "primed".
    void reset() noexcept
    {
        if (control_ != nullptr)
        {
            control_->inputSeq.store (0, std::memory_order_relaxed);
            control_->outputSeq.store (0, std::memory_order_relaxed);
            control_->status.store (static_cast<std::uint32_t> (RtLaneOutput::Silence), std::memory_order_relaxed);

            for (Slot& s : inputSlots_)
                if (s.version != nullptr)
                    s.version->store (0, std::memory_order_relaxed);
            for (Slot& s : outputSlots_)
                if (s.version != nullptr)
                    s.version->store (0, std::memory_order_relaxed);
        }

        resetEndpointState();
    }

    [[nodiscard]] const RtLaneConfig& config() const noexcept { return cfg_; }

    // ── AUDIO THREAD ONLY ───────────────────────────────────────────────────────────────────────────
    // Write Block N's input, publish it, then read Block N-1's output (fail-open). Never blocks/allocates.
    RtLaneExchangeResult exchangeBlock (const float* const* inputChannels, int numInputChannels,
                                        int numFrames,
                                        std::span<const Event> inputEvents,
                                        float* const* outputChannels, int numOutputChannels) noexcept YESDAW_RT_HOT
    {
        RtLaneExchangeResult res;

        const int ch = channels_;
        const int nf = std::clamp (numFrames, 0, maxBlock_);
        YESDAW_RT_ASSERT (numInputChannels >= ch && numOutputChannels >= ch);
        (void) numInputChannels;
        (void) numOutputChannels;

        const std::uint64_t n = inputBlocksSubmitted_;

        // (1) write input Block N into the UNpublished slot, then (2) release-store the input seq counter.
        writeInputSlot (inputSlots_[static_cast<std::size_t> (n & 1u)], n, inputChannels, ch, nf, inputEvents);
        control_->inputSeq.store (n + 1, std::memory_order_release);
        inputBlocksSubmitted_ = n + 1;
        res.inputBlockIndex = n;

        // (3) read Block N-1's output DETERMINISTICALLY (exactly one Block of latency, for PDC), failing
        //     open within the Block budget. Block 0 has no predecessor -> it primes with a miss.
        const bool          hasExpected   = (n >= 1);
        const std::uint64_t expectedBlock = hasExpected ? (n - 1) : 0;
        readOutputFailOpen (outputChannels, ch, nf, hasExpected, expectedBlock, res);

        control_->status.store (static_cast<std::uint32_t> (res.source), std::memory_order_relaxed);
        return res;
    }

    // ── CHILD THREAD ONLY ───────────────────────────────────────────────────────────────────────────
    // Poll the input seq counter; if a new input Block is available, read it, run `process`, and publish
    // the output. Returns true iff it produced an output this call. May poll/spin — never on the audio
    // thread. `process` is: void(std::span<const Event>, const float* const* in, float* const* out,
    //                              int channels, int numFrames).
    template <typename ProcessFn>
    bool pollOnce (ProcessFn&& process) noexcept
    {
        const std::uint64_t s = control_->inputSeq.load (std::memory_order_acquire);
        if (s == 0)
            return false;                                   // nothing published yet

        const std::uint64_t newest = s - 1;
        if (childHasProcessed_ && newest <= lastProcessedInput_)
            return false;                                   // no new input Block since last time

        Slot& in = inputSlots_[static_cast<std::size_t> (newest & 1u)];

        // Seqlock-validated read of the newest input slot.
        const std::uint64_t v1 = in.version->load (std::memory_order_acquire);
        if ((v1 & 1u) != 0u)
            return false;                                   // writer mid-write -> try again later

        const std::uint64_t blockIndex = loadU64 (in.words, kHdrBlockLo);
        const int           inFrames   = clampFrames (in.words[kHdrNumFrames].load (std::memory_order_relaxed));
        const std::uint32_t evCount    = clampEvents (in.words[kHdrEventCount].load (std::memory_order_relaxed));

        for (int c = 0; c < channels_; ++c)
            for (int f = 0; f < inFrames; ++f)
                childInPtrs_[c][f] = loadFloatWord (in.words, audioWord (c, f));

        for (std::uint32_t i = 0; i < evCount; ++i)
            childEvents_[i] = loadEventWord (in.words, eventBase_ + static_cast<std::size_t> (i) * kEventWords);

        // Acquire fence: pin the relaxed payload loads ABOVE the v2 re-read so a concurrent lap can't
        // sink a payload load past it (the portable-seqlock requirement). v2 may then be relaxed.
        std::atomic_thread_fence (std::memory_order_acquire);
        const std::uint64_t v2 = in.version->load (std::memory_order_relaxed);
        if (v1 != v2 || blockIndex != newest)
            return false;                                   // torn / lapped read -> try again later

        if (childHasProcessed_ && blockIndex <= lastProcessedInput_)
            return false;                                   // already produced this one

        process (std::span<const Event> (childEvents_.data(), evCount),
                 childInPtrs_.data(), childOutPtrs_.data(), channels_, inFrames);

        writeOutputSlot (outputSlots_[static_cast<std::size_t> (blockIndex & 1u)],
                         blockIndex, childOutPtrs_.data(), channels_, inFrames, evCount);
        control_->outputSeq.store (blockIndex + 1, std::memory_order_release);

        lastProcessedInput_ = blockIndex;
        childHasProcessed_  = true;
        return true;
    }

    // ── Diagnostics / control words (any thread) ────────────────────────────────────────────────────
    [[nodiscard]] std::uint64_t inputSeq()  const noexcept { return control_ != nullptr ? control_->inputSeq.load (std::memory_order_acquire) : 0; }
    [[nodiscard]] std::uint64_t outputSeq() const noexcept { return control_ != nullptr ? control_->outputSeq.load (std::memory_order_acquire) : 0; }
    [[nodiscard]] RtLaneOutput  lastStatus() const noexcept
    {
        return control_ != nullptr
             ? static_cast<RtLaneOutput> (control_->status.load (std::memory_order_relaxed))
             : RtLaneOutput::Silence;
    }
    // Transient, self-clearing: the audio-thread bypass latch sets after repeated misses and CLEARS on the
    // next Fresh Block. It is the branch-only fail-open signal, NOT the authoritative crash verdict — the
    // future Plugin host coordinator must drive kill/blacklist/recompile from its own watchdog timer
    // (ADR-0015), not from this flag (a hung child never produces output but also never "crashes" here).
    [[nodiscard]] bool bypassActive() const noexcept { return bypassLatched_; }

    [[nodiscard]] std::int64_t validatedLatencySamples() const noexcept
    {
        return control_ != nullptr ? control_->validatedLatency.load (std::memory_order_acquire) : 0;
    }
    void setValidatedLatencySamples (std::int64_t latency) noexcept
    {
        if (control_ != nullptr)
            control_->validatedLatency.store (latency, std::memory_order_release);
    }

private:
    // ── Region layout (a slot's words: header | audio[channels*maxBlock] | events[maxEvents]) ─────────
    static constexpr std::size_t kHdrBlockLo    = 0;   // blockIndex low  32 bits
    static constexpr std::size_t kHdrBlockHi    = 1;   // blockIndex high 32 bits
    static constexpr std::size_t kHdrNumFrames  = 2;
    static constexpr std::size_t kHdrEventCount = 3;
    static constexpr std::size_t kHdrWords      = 4;

    static_assert (sizeof (Event) % sizeof (std::uint32_t) == 0, "Event must pack into 32-bit ring words");
    static constexpr std::size_t kEventWords = sizeof (Event) / sizeof (std::uint32_t);

    static constexpr std::uint32_t kSharedMagic = 0x59445254u;   // "YDRT"
    static constexpr std::uint32_t kSharedVersion = 1;

    // The "control word block" (ADR-0015): the seam's atomics. Grouped to mirror the shared-memory layout.
    struct Control
    {
        std::atomic<std::uint64_t> inputSeq        { 0 };   // # input Blocks published by the audio thread
        std::atomic<std::uint64_t> outputSeq       { 0 };   // # output Blocks published by the child
        std::atomic<std::int64_t>  validatedLatency{ 0 };   // child/plugin-reported latency, validated upstream
        std::atomic<std::uint32_t> status          { 0 };   // last RtLaneOutput delivered to the audio thread
    };

    struct SharedHeader
    {
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        std::uint64_t regionBytes = 0;
        std::uint32_t channels = 0;
        std::uint32_t maxBlockSize = 0;
        std::uint32_t maxEventsPerBlock = 0;
        std::uint32_t lastGoodHoldBlocks = 0;
        std::uint32_t bypassAfterMisses = 0;
        std::uint32_t totalSlotWords = 0;
        std::uint32_t audioBaseWords = 0;
        std::uint32_t eventBaseWords = 0;
        std::uint32_t reserved = 0;
        std::uint64_t controlOffset = 0;
        std::array<std::uint64_t, 2> inputVersionOffsets {};
        std::array<std::uint64_t, 2> inputWordsOffsets {};
        std::array<std::uint64_t, 2> outputVersionOffsets {};
        std::array<std::uint64_t, 2> outputWordsOffsets {};
    };

    struct Layout
    {
        std::size_t regionBytes = 0;
        std::size_t controlOffset = 0;
        std::array<std::size_t, 2> inputVersionOffsets {};
        std::array<std::size_t, 2> inputWordsOffsets {};
        std::array<std::size_t, 2> outputVersionOffsets {};
        std::array<std::size_t, 2> outputWordsOffsets {};
    };

    static std::size_t alignUp (std::size_t value, std::size_t alignment) noexcept
    {
        const std::size_t mask = alignment - 1u;
        return (value + mask) & ~mask;
    }

    static Layout computeLayout (std::size_t totalWords) noexcept
    {
        Layout layout;
        std::size_t offset = alignUp (sizeof (SharedHeader), alignof (Control));
        layout.controlOffset = offset;
        offset += sizeof (Control);

        auto addSlot = [&] (std::size_t& versionOffset, std::size_t& wordsOffset)
        {
            offset = alignUp (offset, alignof (std::atomic<std::uint64_t>));
            versionOffset = offset;
            offset += sizeof (std::atomic<std::uint64_t>);
            offset = alignUp (offset, alignof (std::atomic<std::uint32_t>));
            wordsOffset = offset;
            offset += totalWords * sizeof (std::atomic<std::uint32_t>);
        };

        for (std::size_t i = 0; i < 2; ++i)
            addSlot (layout.inputVersionOffsets[i], layout.inputWordsOffsets[i]);
        for (std::size_t i = 0; i < 2; ++i)
            addSlot (layout.outputVersionOffsets[i], layout.outputWordsOffsets[i]);

        layout.regionBytes = alignUp (offset, alignof (std::max_align_t));
        return layout;
    }

    class MappedRegion
    {
    public:
        MappedRegion() = default;
        ~MappedRegion() { reset(); }

        MappedRegion (const MappedRegion&) = delete;
        MappedRegion& operator= (const MappedRegion&) = delete;

        MappedRegion (MappedRegion&& other) noexcept { moveFrom (other); }
        MappedRegion& operator= (MappedRegion&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                moveFrom (other);
            }
            return *this;
        }

        void create (std::string_view name, std::size_t bytes)
        {
            reset();
            name_ = std::string (name);
            size_ = bytes;

#if defined (_WIN32)
            handle_ = ::CreateFileMappingA (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                            static_cast<DWORD> (bytes >> 32),
                                            static_cast<DWORD> (bytes & 0xffffffffu),
                                            name_.c_str());
            if (handle_ == nullptr)
                throw std::runtime_error ("RtLaneRing CreateFileMapping failed");

            data_ = static_cast<std::byte*> (::MapViewOfFile (handle_, FILE_MAP_ALL_ACCESS, 0, 0, bytes));
            if (data_ == nullptr)
                throw std::runtime_error ("RtLaneRing MapViewOfFile failed");
#else
            fd_ = ::shm_open (name_.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
            if (fd_ < 0)
                throw std::runtime_error ("RtLaneRing shm_open create failed");

            owner_ = true;
            if (::ftruncate (fd_, static_cast<off_t> (bytes)) != 0)
                throw std::runtime_error ("RtLaneRing ftruncate failed");

            void* mapped = ::mmap (nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
            if (mapped == MAP_FAILED)
                throw std::runtime_error ("RtLaneRing mmap create failed");
            data_ = static_cast<std::byte*> (mapped);
#endif

            std::fill_n (data_, size_, std::byte { 0 });
        }

        bool open (std::string_view name)
        {
            reset();
            name_ = std::string (name);

#if defined (_WIN32)
            handle_ = ::OpenFileMappingA (FILE_MAP_ALL_ACCESS, FALSE, name_.c_str());
            if (handle_ == nullptr)
                return false;

            data_ = static_cast<std::byte*> (::MapViewOfFile (handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
            if (data_ == nullptr)
            {
                reset();
                return false;
            }

            const auto* const header = reinterpret_cast<const SharedHeader*> (data_);
            size_ = static_cast<std::size_t> (header->regionBytes);
#else
            fd_ = ::shm_open (name_.c_str(), O_RDWR, 0600);
            if (fd_ < 0)
                return false;

            struct stat st {};
            if (::fstat (fd_, &st) != 0 || st.st_size <= 0)
            {
                reset();
                return false;
            }

            size_ = static_cast<std::size_t> (st.st_size);
            void* mapped = ::mmap (nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
            if (mapped == MAP_FAILED)
            {
                reset();
                return false;
            }
            data_ = static_cast<std::byte*> (mapped);
#endif

            return true;
        }

        void reset() noexcept
        {
#if defined (_WIN32)
            if (data_ != nullptr)
                ::UnmapViewOfFile (data_);
            if (handle_ != nullptr)
                ::CloseHandle (handle_);
            handle_ = nullptr;
#else
            if (data_ != nullptr)
                ::munmap (data_, size_);
            if (fd_ >= 0)
                ::close (fd_);
            if (owner_ && ! name_.empty())
                ::shm_unlink (name_.c_str());
            fd_ = -1;
            owner_ = false;
#endif
            data_ = nullptr;
            size_ = 0;
            name_.clear();
        }

        [[nodiscard]] bool isMapped() const noexcept { return data_ != nullptr; }
        [[nodiscard]] std::byte* data() const noexcept { return data_; }
        [[nodiscard]] std::size_t size() const noexcept { return size_; }

    private:
        void moveFrom (MappedRegion& other) noexcept
        {
            data_ = other.data_;
            size_ = other.size_;
            name_ = std::move (other.name_);
#if defined (_WIN32)
            handle_ = other.handle_;
            other.handle_ = nullptr;
#else
            fd_ = other.fd_;
            owner_ = other.owner_;
            other.fd_ = -1;
            other.owner_ = false;
#endif
            other.data_ = nullptr;
            other.size_ = 0;
        }

        std::byte* data_ = nullptr;
        std::size_t size_ = 0;
        std::string name_;
#if defined (_WIN32)
        HANDLE handle_ = nullptr;
#else
        int fd_ = -1;
        bool owner_ = false;
#endif
    };

    struct Slot
    {
        // Seqlock version: even == stable generation, odd == a write is in progress.
        std::atomic<std::uint64_t>* version = nullptr;
        // Payload words, all relaxed-atomic so a concurrent lapping write is well-defined, not UB.
        std::atomic<std::uint32_t>* words = nullptr;

        void bind (std::byte* base, std::size_t versionOffset, std::size_t wordsOffset)
        {
            version = reinterpret_cast<std::atomic<std::uint64_t>*> (base + versionOffset);
            words = reinterpret_cast<std::atomic<std::uint32_t>*> (base + wordsOffset);
        }
    };

    void bindSharedLayout (const Layout& layout) noexcept
    {
        std::byte* const base = region_.data();
        control_ = reinterpret_cast<Control*> (base + layout.controlOffset);
        for (std::size_t i = 0; i < 2; ++i)
        {
            inputSlots_[i].bind (base, layout.inputVersionOffsets[i], layout.inputWordsOffsets[i]);
            outputSlots_[i].bind (base, layout.outputVersionOffsets[i], layout.outputWordsOffsets[i]);
        }
    }

    void initialiseSharedLayout (const Layout& layout)
    {
        new (control_) Control {};

        auto initialiseSlot = [this] (Slot& slot)
        {
            new (slot.version) std::atomic<std::uint64_t> { 0 };
            for (std::size_t i = 0; i < totalWords_; ++i)
                new (&slot.words[i]) std::atomic<std::uint32_t> { 0 };
        };

        for (Slot& s : inputSlots_)  initialiseSlot (s);
        for (Slot& s : outputSlots_) initialiseSlot (s);

        auto* const header = reinterpret_cast<SharedHeader*> (region_.data());
        header->magic = kSharedMagic;
        header->version = kSharedVersion;
        header->regionBytes = layout.regionBytes;
        header->channels = static_cast<std::uint32_t> (channels_);
        header->maxBlockSize = static_cast<std::uint32_t> (maxBlock_);
        header->maxEventsPerBlock = maxEvents_;
        header->lastGoodHoldBlocks = cfg_.lastGoodHoldBlocks;
        header->bypassAfterMisses = cfg_.bypassAfterMisses;
        header->totalSlotWords = static_cast<std::uint32_t> (totalWords_);
        header->audioBaseWords = static_cast<std::uint32_t> (audioBase_);
        header->eventBaseWords = static_cast<std::uint32_t> (eventBase_);
        header->controlOffset = layout.controlOffset;
        for (std::size_t i = 0; i < 2; ++i)
        {
            header->inputVersionOffsets[i] = layout.inputVersionOffsets[i];
            header->inputWordsOffsets[i] = layout.inputWordsOffsets[i];
            header->outputVersionOffsets[i] = layout.outputVersionOffsets[i];
            header->outputWordsOffsets[i] = layout.outputWordsOffsets[i];
        }
    }

    void prepareEndpointScratch()
    {
        childAudio_.assign (static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_), 0.0f);
        childOut_.assign   (static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_), 0.0f);
        childInPtrs_.resize  (static_cast<std::size_t> (channels_));
        childOutPtrs_.resize (static_cast<std::size_t> (channels_));
        for (int c = 0; c < channels_; ++c)
        {
            childInPtrs_[c]  = childAudio_.data() + static_cast<std::size_t> (c) * maxBlock_;
            childOutPtrs_[c] = childOut_.data()   + static_cast<std::size_t> (c) * maxBlock_;
        }
        childEvents_.assign (std::max<std::size_t> (1, maxEvents_), Event{});
        lastGood_.assign (static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_), 0.0f);
        resetEndpointState();
    }

    void resetEndpointState() noexcept
    {
        inputBlocksSubmitted_ = 0;
        lastProcessedInput_ = 0;
        childHasProcessed_ = false;
        consecutiveMisses_ = 0;
        bypassLatched_ = false;
        lastGoodValid_ = false;
        lastGoodNumFrames_ = 0;
    }

    [[nodiscard]] int clampFrames (std::uint32_t nf) const noexcept
    {
        return static_cast<int> (std::min (nf, static_cast<std::uint32_t> (maxBlock_)));
    }
    [[nodiscard]] std::uint32_t clampEvents (std::uint32_t ec) const noexcept
    {
        return std::min (ec, maxEvents_);
    }
    [[nodiscard]] std::size_t audioWord (int channel, int frame) const noexcept
    {
        return audioBase_ + static_cast<std::size_t> (channel) * static_cast<std::size_t> (maxBlock_)
             + static_cast<std::size_t> (frame);
    }

    static float loadFloatWord (const std::atomic<std::uint32_t>* w, std::size_t i) noexcept
    {
        return std::bit_cast<float> (w[i].load (std::memory_order_relaxed));
    }
    static void storeFloatWord (std::atomic<std::uint32_t>* w, std::size_t i, float v) noexcept
    {
        w[i].store (std::bit_cast<std::uint32_t> (v), std::memory_order_relaxed);
    }
    static std::uint64_t loadU64 (const std::atomic<std::uint32_t>* w, std::size_t base) noexcept
    {
        return static_cast<std::uint64_t> (w[base].load (std::memory_order_relaxed))
             | (static_cast<std::uint64_t> (w[base + 1].load (std::memory_order_relaxed)) << 32);
    }
    static void storeU64 (std::atomic<std::uint32_t>* w, std::size_t base, std::uint64_t v) noexcept
    {
        w[base].store     (static_cast<std::uint32_t> (v & 0xffffffffu), std::memory_order_relaxed);
        w[base + 1].store (static_cast<std::uint32_t> (v >> 32),         std::memory_order_relaxed);
    }
    // Events are trivially copyable (ADR-0009) but NOT trivial (NSDMIs), so memcpy trips GCC's
    // -Wclass-memaccess; std::bit_cast packs/unpacks the fixed-size Event to/from ring words cleanly.
    using EventWords = std::array<std::uint32_t, kEventWords>;
    static_assert (sizeof (EventWords) == sizeof (Event), "Event must bit_cast losslessly to ring words");

    static Event loadEventWord (const std::atomic<std::uint32_t>* w, std::size_t base) noexcept
    {
        EventWords tmp{};
        for (std::size_t i = 0; i < kEventWords; ++i)
            tmp[i] = w[base + i].load (std::memory_order_relaxed);
        return std::bit_cast<Event> (tmp);
    }
    static void storeEventWord (std::atomic<std::uint32_t>* w, std::size_t base, const Event& e) noexcept
    {
        const EventWords tmp = std::bit_cast<EventWords> (e);
        for (std::size_t i = 0; i < kEventWords; ++i)
            w[base + i].store (tmp[i], std::memory_order_relaxed);
    }

    // Single-writer seqlock write of a slot: bump version odd, write payload, bump version even.
    // The fences make the seqlock portable (the Boehm result): without the release fence below, the
    // relaxed payload stores could become visible BEFORE the odd "write in progress" marker, so a reader
    // could read new payload while still seeing the old even version. (TSan can't see this — it treats
    // relaxed atomics as race-free — but it is real on weaker memory models and in shared memory.)
    void beginWrite (Slot& s) noexcept
    {
        const std::uint64_t stable = s.version->load (std::memory_order_relaxed);
        s.version->store (stable + 1, std::memory_order_relaxed);   // mark odd: write in progress
        std::atomic_thread_fence (std::memory_order_release);      // odd version visible BEFORE any payload store
    }
    void endWrite (Slot& s) noexcept
    {
        const std::uint64_t odd = s.version->load (std::memory_order_relaxed);
        s.version->store (odd + 1, std::memory_order_release);      // payload visible BEFORE the new even version
    }

    void writeInputSlot (Slot& s, std::uint64_t blockIndex,
                         const float* const* audio, int channels, int numFrames,
                         std::span<const Event> events) noexcept
    {
        beginWrite (s);
        storeU64 (s.words, kHdrBlockLo, blockIndex);
        s.words[kHdrNumFrames].store (static_cast<std::uint32_t> (numFrames), std::memory_order_relaxed);
        const std::uint32_t ec = clampEvents (static_cast<std::uint32_t> (events.size()));
        s.words[kHdrEventCount].store (ec, std::memory_order_relaxed);
        for (int c = 0; c < channels; ++c)
            for (int f = 0; f < numFrames; ++f)
                storeFloatWord (s.words, audioWord (c, f), audio[c][f]);
        for (std::uint32_t i = 0; i < ec; ++i)
            storeEventWord (s.words, eventBase_ + static_cast<std::size_t> (i) * kEventWords, events[i]);
        endWrite (s);
    }

    void writeOutputSlot (Slot& s, std::uint64_t blockIndex,
                          const float* const* audio, int channels, int numFrames,
                          std::uint32_t eventsApplied) noexcept
    {
        beginWrite (s);
        storeU64 (s.words, kHdrBlockLo, blockIndex);
        s.words[kHdrNumFrames].store (static_cast<std::uint32_t> (numFrames), std::memory_order_relaxed);
        s.words[kHdrEventCount].store (eventsApplied, std::memory_order_relaxed);
        for (int c = 0; c < channels; ++c)
            for (int f = 0; f < numFrames; ++f)
                storeFloatWord (s.words, audioWord (c, f), audio[c][f]);
        endWrite (s);
    }

    // The audio-thread read with the fail-open ladder (last-good -> silence -> bypass). Branch-only.
    // Delivers EXACTLY `expectedBlock` (== Block N-1) when the child has published it, else fails open —
    // so a hosted plugin's latency is a deterministic single Block (ADR-0015 / ADR-0007 PDC).
    void readOutputFailOpen (float* const* out, int channels, int numFrames,
                             bool hasExpected, std::uint64_t expectedBlock, RtLaneExchangeResult& res) noexcept
    {
        bool          fresh    = false;
        std::uint32_t gotEvents = 0;
        std::uint32_t gotFrames = 0;

        // The "output ready counter" (ADR-0015): acquire-load it; the child has published Block
        // `expectedBlock` once ready > expectedBlock (blocks 0..expectedBlock are all published).
        const std::uint64_t ready = control_->outputSeq.load (std::memory_order_acquire);
        if (hasExpected && ready > expectedBlock)
        {
            Slot& s = outputSlots_[static_cast<std::size_t> (expectedBlock & 1u)];
            const std::uint64_t v1 = s.version->load (std::memory_order_acquire);
            if ((v1 & 1u) == 0u)
            {
                const std::uint64_t bi = loadU64 (s.words, kHdrBlockLo);
                const int      sf = clampFrames (s.words[kHdrNumFrames].load (std::memory_order_relaxed));
                const std::uint32_t ev = s.words[kHdrEventCount].load (std::memory_order_relaxed);
                const int      wn = std::min (sf, numFrames);

                for (int c = 0; c < channels; ++c)
                    for (int f = 0; f < wn; ++f)
                        out[c][f] = loadFloatWord (s.words, audioWord (c, f));

                // Acquire fence: pin the relaxed payload loads ABOVE the v2 re-read (portable seqlock).
                std::atomic_thread_fence (std::memory_order_acquire);
                const std::uint64_t v2 = s.version->load (std::memory_order_relaxed);
                if (v1 == v2 && bi == expectedBlock)   // consistent snapshot AND the Block we wanted
                {
                    fresh = true;
                    gotEvents = ev;
                    gotFrames = static_cast<std::uint32_t> (wn);
                    for (int c = 0; c < channels; ++c)
                        for (int f = wn; f < numFrames; ++f)
                            out[c][f] = 0.0f;
                }
            }
        }

        if (fresh)
        {
            res.source           = RtLaneOutput::Fresh;
            res.deliveredFresh   = true;
            res.outputBlockIndex = expectedBlock;
            res.outputEventCount = gotEvents;
            res.outputNumFrames  = gotFrames;

            consecutiveMisses_        = 0;
            bypassLatched_            = false;

            // Capture this good output so a later miss can re-serve it (last-good).
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < static_cast<int> (gotFrames); ++f)
                    lastGood_[static_cast<std::size_t> (c) * maxBlock_ + static_cast<std::size_t> (f)] = out[c][f];
            lastGoodValid_     = true;
            lastGoodNumFrames_ = gotFrames;
            return;
        }

        // Miss: fail open along the ladder (ADR-0015) — last-good -> silence -> bypass, branch-only.
        ++consecutiveMisses_;
        if (consecutiveMisses_ >= cfg_.bypassAfterMisses)
            bypassLatched_ = true;

        if (bypassLatched_)
        {
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < numFrames; ++f)
                    out[c][f] = 0.0f;
            res.source = RtLaneOutput::Bypass;
        }
        else if (lastGoodValid_ && consecutiveMisses_ <= cfg_.lastGoodHoldBlocks)
        {
            const int wn = std::min (static_cast<int> (lastGoodNumFrames_), numFrames);
            for (int c = 0; c < channels; ++c)
            {
                for (int f = 0; f < wn; ++f)
                    out[c][f] = lastGood_[static_cast<std::size_t> (c) * maxBlock_ + static_cast<std::size_t> (f)];
                for (int f = wn; f < numFrames; ++f)
                    out[c][f] = 0.0f;
            }
            res.source           = RtLaneOutput::LastGood;
            res.outputNumFrames  = static_cast<std::uint32_t> (wn);
        }
        else
        {
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < numFrames; ++f)
                    out[c][f] = 0.0f;
            res.source = RtLaneOutput::Silence;
        }
    }

    RtLaneConfig cfg_{};
    int           channels_  = 1;
    int           maxBlock_  = 512;
    std::uint32_t maxEvents_ = 64;

    std::size_t audioBase_  = kHdrWords;
    std::size_t eventBase_  = kHdrWords;
    std::size_t totalWords_ = kHdrWords;

    MappedRegion           region_;
    std::string            sharedMemoryName_;
    Control*               control_ = nullptr;
    std::array<Slot, 2>    inputSlots_;
    std::array<Slot, 2>    outputSlots_;

    // Audio-thread-local state (touched only by exchangeBlock).
    std::uint64_t inputBlocksSubmitted_ = 0;
    std::uint32_t consecutiveMisses_    = 0;
    bool          bypassLatched_        = false;
    std::vector<float> lastGood_;
    bool          lastGoodValid_           = false;
    std::uint32_t lastGoodNumFrames_       = 0;

    // Child-thread-local state (touched only by pollOnce).
    std::uint64_t lastProcessedInput_ = 0;
    bool          childHasProcessed_  = false;
    std::vector<float>  childAudio_;
    std::vector<float>  childOut_;
    std::vector<float*> childInPtrs_;
    std::vector<float*> childOutPtrs_;
    std::vector<Event>  childEvents_;
};

} // namespace yesdaw::engine
