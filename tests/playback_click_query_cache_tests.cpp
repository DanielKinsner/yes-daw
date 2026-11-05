// Independent whole-TU cache contract proof. SOURCE ONLY until the owning run is reviewed.
// Requires the reviewed private PlaybackEngine overlay; no production test API is added.
#include "engine/PlaybackEngine.h"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace yesdaw::engine::click_cache_proof {
struct Access
{
    using State = PlaybackEngine::ClickCacheState;
    struct Entry
    {
        State state = State::Empty;
        std::int64_t first = 0, end = 0;
        std::size_t rows = 0;
        TimeLookupStatus terminal = TimeLookupStatus::Ok;
        bool terminalFound = false;
        friend bool operator== (const Entry&, const Entry&) = default;
    };
    struct Selection { bool recording = false, replay = false; std::size_t slot = 0; };
    static bool supported() noexcept { return PlaybackEngine::kClickCacheContextSupported; }
    static std::size_t capacity (const PlaybackEngine& e) { return e.clickCacheRowsPerEntry_; }
    static std::size_t requested (const PlaybackEngine& e)
    {
        return sizeof (e.clickCacheEntries_) + sizeof (e.clickCacheRows_)
            + sizeof (e.clickCacheRowsPerEntry_) + sizeof (e.clickCacheNextEntry_)
            + e.clickCacheRows_.capacity() * sizeof (PlaybackEngine::CachedClickOnset);
    }
    static std::array<Entry, 8> entries (const PlaybackEngine& e)
    {
        std::array<Entry, 8> out {};
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const auto& x = e.clickCacheEntries_[i];
            out[i] = { x.state, x.firstFrame, x.endFrame, x.rowCount };
            if (x.rowCount != 0 && x.rowCount <= e.clickCacheRowsPerEntry_)
            {
                const auto& last = e.clickCacheRows_[i * e.clickCacheRowsPerEntry_ + x.rowCount - 1];
                out[i].terminal = last.status;
                out[i].terminalFound = last.found;
            }
        }
        return out;
    }
    static std::optional<Entry> find (const PlaybackEngine& e, std::int64_t first, std::int64_t end)
    {
        for (const auto& x : entries (e))
            if (x.state != State::Empty && x.first == first && x.end == end) return x;
        return std::nullopt;
    }
    // This deliberately calls the real mutating selector, rather than predicting its result.
    static Selection select (PlaybackEngine& e, const MusicalClickMap* clock,
        std::int64_t minimum, std::int64_t endOnset, std::int64_t first, std::int64_t end)
    {
        const auto x = e.selectClickCache (clock, minimum, endOnset, first, end);
        return { x.entry != nullptr && ! x.replay, x.replay,
            x.entry == nullptr ? 0u : static_cast<std::size_t> (x.entry - e.clickCacheEntries_.data()) };
    }
    static std::uint64_t recordedControl (const PlaybackEngine& e, std::size_t slot) noexcept
    { return e.clickCacheEntries_[slot].context.control; }
    static void overlay (PlaybackEngine& e, float* const* out, int frames, std::int64_t start,
        const MusicalClickMap* clock, std::int64_t minimum = 0, std::int64_t endOnset = INT64_MAX)
    { e.overlayClickMap (out, 2, 0, frames, start, clock, minimum, endOnset); }
};
} // namespace yesdaw::engine::click_cache_proof

namespace {
using namespace yesdaw::engine;
using Access = click_cache_proof::Access;
using Stereo = std::array<std::vector<float>, 2>;
constexpr int clickLength = 240;
constexpr std::int64_t unlimited = INT64_MAX;

// Test-side numeric controls only. Sticky flags, errno and x87 state are not an output contract.
struct NumericControl
{
    std::uint64_t control = 0;
    NumericControl() noexcept
    {
#if defined(__x86_64__) || defined(_M_X64)
        control = _mm_getcsr() & ~std::uint64_t { 0x3f };
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (control));
#endif
    }
    static void write (std::uint64_t value) noexcept
    {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_setcsr (static_cast<unsigned int> (value) | (_mm_getcsr() & 0x3fu));
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__ ("msr fpcr, %0" : : "r" (value) : "memory");
#else
        (void) value;
#endif
    }
    void restore() const noexcept { write (control); }
    void installDefault() const noexcept
    {
#if defined(__x86_64__) || defined(_M_X64)
        write ((control & ~std::uint64_t { 0x6000 }) | 0x1f80u);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        write (control & ~std::uint64_t { 0xc09f00 });
#endif
    }
};
struct RestoreNumericControl
{
    NumericControl entry;
    RestoreNumericControl() noexcept { entry.installDefault(); }
    ~RestoreNumericControl() { entry.restore(); }
};
struct ControlledSelection { Access::Selection selection; std::uint64_t recorded = 0; };
// No float arithmetic or Window runs between these writes, even when this integer-only call
// tests refusal of an unmasked trap. Keep caller float work outside this non-inlined region.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
ControlledSelection selectWithControl (PlaybackEngine& e, const MusicalClickMap* clock,
    std::uint64_t changed, const NumericControl& baseline) noexcept
{
    NumericControl::write (changed);
    const auto selected = Access::select (e, clock, 0, unlimited, 261, 532);
    const auto recorded = selected.recording ? Access::recordedControl (e, selected.slot) : 0u;
    baseline.restore(); // Restore before returning to any assertion, allocation or actual lookup.
    return { selected, recorded };
}

Stereo zeros (int frames)
{ return { std::vector<float> (static_cast<std::size_t> (frames)), std::vector<float> (static_cast<std::size_t> (frames)) }; }
bool sameBits (const Stereo& a, const Stereo& b)
{
    for (std::size_t c = 0; c < 2; ++c)
    {
        if (a[c].size() != b[c].size()) return false;
        for (std::size_t i = 0; i < a[c].size(); ++i)
            if (std::bit_cast<std::uint32_t> (a[c][i]) != std::bit_cast<std::uint32_t> (b[c][i])) return false;
    }
    return true;
}
struct Tables
{
    std::array<std::array<float, clickLength>, 2> samples {};
    Tables()
    {
        for (int accent = 0; accent != 2; ++accent)
            for (int n = 0; n != clickLength; ++n)
            {
                const double seconds = static_cast<double> (n) / 48000.0;
                const double carrier = std::sin (6.283185307179586 * (accent != 0 ? 1500.0 : 1000.0) * seconds);
                samples[static_cast<std::size_t> (accent)][static_cast<std::size_t> (n)] =
                    static_cast<float> (carrier * std::exp (-600.0 * seconds) * 0.5);
            }
    }
};
struct Reference { Stereo pcm; TimeLookupStatus status = TimeLookupStatus::Ok; int onsets = 0; };
Reference reference (const MusicalClickMap& clock, const Tables& tables, std::int64_t start,
    int frames, Stereo initial, std::int64_t minimum = 0, std::int64_t endOnset = unlimited)
{
    Reference out { std::move (initial) };
    auto live = clock.onsetsInRange (std::max (minimum, std::max<std::int64_t> (0, start - clickLength + 1)),
                                    std::min (start + frames, endOnset));
    for (int calls = 0; ; ++calls)
    {
        REQUIRE (calls <= frames + clickLength);
        const auto onset = live.next(); // Always public uncached clock; never a cache transcript.
        if (onset.status != TimeLookupStatus::Ok) { out.status = onset.status; break; }
        if (! onset.found) break;
        ++out.onsets;
        for (int n = 0; n < frames; ++n)
        {
            const auto age = start + n - onset.frame;
            if (age < 0 || age >= clickLength) continue;
            const float click = tables.samples[onset.downbeat ? 1u : 0u][static_cast<std::size_t> (age)];
            for (auto& channel : out.pcm)
            {
                volatile float rounded = channel[static_cast<std::size_t> (n)] + click;
                channel[static_cast<std::size_t> (n)] = rounded;
            }
        }
    }
    return out;
}
Project clockProject (double bpm = 11520000.0)
{
    Project p;
    p.sampleRate = { 48000.0 };
    p.tempoMap = { { 0, bpm, TempoCurve::Jump } };
    p.meterMap = { { 0, 7, 4 } };
    return p;
}
std::unique_ptr<PlaybackEngine> transport (const Project& p, int block = 257)
{
    auto e = PlaybackEngine::createTransportOnly (p, block);
    REQUIRE (e != nullptr);
    REQUIRE (e->metronomePreparationStatus() == TimeLookupStatus::Ok);
    REQUIRE (e->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
    REQUIRE (e->setLoop (0, 1000000));
    e->processBlock (nullptr, 0, 0);
    return e;
}
Stereo draw (PlaybackEngine& e, int frames, std::int64_t start, const MusicalClickMap* clock = nullptr,
    std::int64_t minimum = 0, std::int64_t endOnset = unlimited)
{
    auto out = zeros (frames);
    float* pointers[] { out[0].data(), out[1].data() };
    Access::overlay (e, pointers, frames, start, clock != nullptr ? clock : e.metronomeClock().get(), minimum, endOnset);
    return out;
}
Stereo publicRender (PlaybackEngine& e, int frames, int partition)
{
    auto out = zeros (frames), block = zeros (257);
    float* pointers[] { block[0].data(), block[1].data() };
    for (int cursor = 0; cursor < frames;)
    {
        const int count = std::min (partition, frames - cursor);
        e.processBlock (pointers, 2, count);
        for (std::size_t c = 0; c < 2; ++c)
            std::copy_n (block[c].begin(), count, out[c].begin() + cursor);
        cursor += count;
    }
    return out;
}
EntityId identity (std::uint8_t value)
{
    EntityId::StorageBytes bytes {};
    bytes[2] = 0xc1; bytes[7] = value; bytes.back() = 0x5a;
    return EntityId::fromBytes (bytes);
}
Stereo source (int frames)
{
    auto out = zeros (frames);
    for (int n = 0; n < frames; ++n)
    {
        out[0][static_cast<std::size_t> (n)] = static_cast<float> (n % 23 - 11) * 0x1p-9f + 0x1p-14f;
        out[1][static_cast<std::size_t> (n)] = static_cast<float> (n % 19 - 9) * 0x1p-8f - 0x1p-13f;
    }
    return out;
}
std::unique_ptr<PlaybackEngine> graphEngine (const Project& project, int block = 257)
{
    auto p = project;
    p.id = identity (1);
    Asset a; a.id = identity (2); a.contentHash.bytes.fill (0x69);
    a.frames = 1024; a.sampleRate = p.sampleRate; a.channels = 2;
    Track track; track.id = identity (3); track.strip.name = "Independent stereo source";
    Clip clip; clip.id = identity (4); clip.assetId = a.id; clip.trackId = track.id;
    clip.timelineLength = 1024; clip.srcLen = 1024;
    p.assets = { a }; p.tracks = { track }; p.clips = { clip };
    const auto raw = source (1024);
    std::vector<float> interleaved (2048);
    for (std::size_t n = 0; n < 1024; ++n)
        for (std::size_t c = 0; c < 2; ++c) interleaved[2 * n + c] = raw[c][n];
    const DecodedAssetAudio decoded { a.id, a.sampleRate, a.frames, 2, interleaved };
    OfflineRenderOptions options; options.maxBlockSize = block;
    auto made = PlaybackEngine::create (p, std::span<const DecodedAssetAudio> (&decoded, 1), options);
    REQUIRE (made.ok());
    return std::move (made.engine); // The graph owns its normal decoded-source copy.
}
void unsupportedFallback (PlaybackEngine& e, const Tables& tables)
{
    REQUIRE (Access::capacity (e) == 0);
    const auto expected = reference (*e.metronomeClock(), tables, 300, 32, zeros (32));
    REQUIRE (sameBits (draw (e, 32, 300), expected.pcm));
    for (const auto& x : Access::entries (e)) REQUIRE (x.state == Access::State::Empty);
}
} // namespace

TEST_CASE ("exact click-query cache preserves uncached chronology and bounded eligibility",
           "[playback][musical-click][click-query-cache][rtsan]")
{
    using namespace yesdaw::engine;
    RestoreNumericControl restore;
    const Tables tables;
    const auto project = clockProject();

    SECTION ("public stereo graph retains per-onset float addition on cold fill and replay")
    {
        auto owner = graphEngine (project);
        auto& e = *owner;
        REQUIRE (sameBits (publicRender (e, 128, 128), source (128)));
        REQUIRE (e.setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (e.setLoop (0, 128)); REQUIRE (e.locate (0));
        e.processBlock (nullptr, 0, 0);
        const auto expected = reference (*e.metronomeClock(), tables, 0, 128, source (128));
        auto premixed = reference (*e.metronomeClock(), tables, 0, 128, zeros (128)).pcm;
        const auto base = source (128);
        for (std::size_t c = 0; c < 2; ++c)
            for (std::size_t n = 0; n < 128; ++n) premixed[c][n] += base[c][n];
        REQUIRE_FALSE (sameBits (premixed, expected.pcm)); // Must kill a pre-summed-click replacement.
        const NumericControl steady;
        for (int pass = 0; pass != 3; ++pass)
        { steady.restore(); REQUIRE (sameBits (publicRender (e, 128, 128), expected.pcm)); }
        if (Access::supported())
        {
            const auto entry = Access::find (e, 0, 128);
            REQUIRE (entry.has_value()); REQUIRE (entry->state == Access::State::Complete);
            steady.restore(); REQUIRE (Access::select (e, e.metronomeClock().get(), 0, unlimited, 0, 128).replay);
        }
        else unsupportedFallback (e, tables);
    }

    SECTION ("257 distinct actual queries evict the eight-slot ring and resident repeats fill")
    {
        auto e = transport (project);
        if (! Access::supported()) { unsupportedFallback (*e, tables); return; }
        REQUIRE (Access::capacity (*e) == 497);
        REQUIRE (Access::requested (*e) <= 1024u * 1024u);
        for (int q = 0; q != 257; ++q)
        {
            const auto start = 1000 + static_cast<std::int64_t> (q) * 257;
            const auto expected = reference (*e->metronomeClock(), tables, start, 8, zeros (8));
            REQUIRE (sameBits (draw (*e, 8, start), expected.pcm));
            const auto entry = Access::find (*e, start - 239, start + 8);
            REQUIRE (entry.has_value()); REQUIRE (entry->state == Access::State::Seen);
            REQUIRE (entry->rows == 0);
        }
        REQUIRE_FALSE (Access::find (*e, 761, 1008).has_value());
        std::size_t occupied = 0;
        for (const auto& x : Access::entries (*e)) occupied += x.state != Access::State::Empty ? 1u : 0u;
        REQUIRE (occupied == 8);
        constexpr std::int64_t last = 1000 + 256 * 257;
        const auto expected = reference (*e->metronomeClock(), tables, last, 8, zeros (8));
        const NumericControl steady;
        steady.restore(); REQUIRE (sameBits (draw (*e, 8, last), expected.pcm));
        const auto complete = Access::find (*e, last - 239, last + 8);
        REQUIRE (complete.has_value()); REQUIRE (complete->state == Access::State::Complete);
        REQUIRE (complete->rows == 248); // 247 distinct Frames and the actual terminal.
        steady.restore(); REQUIRE (Access::select (*e, e->metronomeClock().get(), 0, unlimited, last - 239, last + 8).replay);
        steady.restore(); REQUIRE (sameBits (draw (*e, 8, last), expected.pcm));
    }

    SECTION ("actual empty and successful-prefix failure terminals survive warm replay")
    {
        for (const bool failure : { false, true })
        {
            auto p = clockProject (failure ? std::numeric_limits<double>::denorm_min() : 120.0);
            auto e = transport (p);
            if (! Access::supported()) { unsupportedFallback (*e, tables); continue; }
            const std::int64_t start = failure ? 0 : 1000;
            const auto expected = reference (*e->metronomeClock(), tables, start, 32, zeros (32));
            REQUIRE (expected.status == (failure ? TimeLookupStatus::NumericDomain : TimeLookupStatus::Ok));
            REQUIRE (expected.onsets == (failure ? 1 : 0));
            if (failure) REQUIRE_FALSE (sameBits (expected.pcm, zeros (32)));
            const NumericControl steady;
            for (int pass = 0; pass != 3; ++pass)
            {
                steady.restore(); REQUIRE (sameBits (draw (*e, 32, start), expected.pcm));
                REQUIRE (e->metronomeLookupStatus() == expected.status);
            }
            const auto first = std::max<std::int64_t> (0, start - 239);
            const auto entry = Access::find (*e, first, start + 32);
            REQUIRE (entry.has_value()); REQUIRE (entry->state == Access::State::Complete);
            REQUIRE (entry->rows == (failure ? 2u : 1u));
            REQUIRE (entry->terminal == expected.status); REQUIRE_FALSE (entry->terminalFound);
            steady.restore(); REQUIRE (Access::select (*e, e->metronomeClock().get(), 0, unlimited, first, start + 32).replay);
        }
    }

    SECTION ("query identity width and bounded preparation refusals retain live enumeration")
    {
        auto e = transport (project);
        const auto expected = reference (*e->metronomeClock(), tables, 500, 32, zeros (32));
        REQUIRE (sameBits (draw (*e, 32, 500), expected.pcm));
        if (! Access::supported()) { unsupportedFallback (*e, tables); return; }
        MusicalClickMap other;
        REQUIRE (MusicalClickMap::build ({ project.tempoMap.data(), project.tempoMap.size() },
            { project.meterMap.data(), project.meterMap.size() }, project.sampleRate, other) == TimeLookupStatus::Ok);
        const auto before = Access::entries (*e);
        for (const auto selection : {
            Access::select (*e, &other, 0, unlimited, 261, 532),
            Access::select (*e, e->metronomeClock().get(), 1, unlimited, 261, 532),
            Access::select (*e, e->metronomeClock().get(), 0, 600, 261, 532),
            Access::select (*e, e->metronomeClock().get(), 0, unlimited, -1, 32),
            Access::select (*e, e->metronomeClock().get(), 0, unlimited, 32, 31),
            Access::select (*e, e->metronomeClock().get(), 0, unlimited, 0, 497),
            Access::select (*e, e->metronomeClock().get(), 0, unlimited, 0, INT64_MAX) })
        { REQUIRE_FALSE (selection.recording); REQUIRE_FALSE (selection.replay); }
        REQUIRE (Access::entries (*e) == before);
        REQUIRE (sameBits (draw (*e, 32, 500, &other), expected.pcm));
        REQUIRE (Access::entries (*e) == before);
        // Maximum legal ordinary query: L-1 history +B Frames +one terminal exactly fills storage.
        const auto full = reference (*e->metronomeClock(), tables, 1000, 257, zeros (257));
        const NumericControl steady;
        for (int pass = 0; pass != 3; ++pass)
        { steady.restore(); REQUIRE (sameBits (draw (*e, 257, 1000), full.pcm)); }
        const auto boundaryEntry = Access::find (*e, 761, 1257);
        REQUIRE (boundaryEntry.has_value()); REQUIRE (boundaryEntry->state == Access::State::Complete);
        REQUIRE (boundaryEntry->rows == Access::capacity (*e));
        const auto beforeDisabled = Access::entries (*e);
        REQUIRE (e->clearLoop()); e->processBlock (nullptr, 0, 0);
        REQUIRE (sameBits (draw (*e, 32, 500), expected.pcm)); REQUIRE (Access::entries (*e) == beforeDisabled);
        auto large = transport (project, 20000); // Bounded ~5MiB shuttle storage, optional cache exceeds1MiB.
        REQUIRE (Access::capacity (*large) == 0);
        REQUIRE (sameBits (draw (*large, 32, 500), expected.pcm));
    }

    SECTION ("native numeric controls refuse unsupported entry modes and identify flush context")
    {
        for (int kind = 0; kind != 3; ++kind)
        {
            auto e = transport (project);
            if (! Access::supported()) { unsupportedFallback (*e, tables); continue; }
            const auto expected = reference (*e->metronomeClock(), tables, 500, 32, zeros (32));
            const NumericControl stable;
            const auto* clock = e->metronomeClock().get();
            for (int pass = 0; pass != 3; ++pass)
            { stable.restore(); REQUIRE (sameBits (draw (*e, 32, 500), expected.pcm)); }
            const auto complete = Access::find (*e, 261, 532);
            REQUIRE (complete.has_value()); REQUIRE (complete->state == Access::State::Complete);
            std::uint64_t changed = stable.control;
#if defined(__x86_64__) || defined(_M_X64)
            if (kind == 0) changed = (changed & ~std::uint64_t { 0x6000 }) | 0x2000u;
            if (kind == 1) changed &= ~std::uint64_t { 0x80 }; // Unmask invalid; no float instruction executes there.
            if (kind == 2) changed ^= 0x8000u;
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
            if (kind == 0) changed = (changed & ~std::uint64_t { 0xc00000 }) | 0x800000u;
            if (kind == 1) changed |= 0x100u;
            if (kind == 2) changed ^= std::uint64_t { 1 } << 24u;
#endif
            const auto selected = selectWithControl (*e, clock, changed, stable);
            REQUIRE_FALSE (selected.selection.replay);
            if (kind < 2) REQUIRE_FALSE (selected.selection.recording);
            else { REQUIRE (selected.selection.recording); REQUIRE (selected.recorded == changed); }
            // Both actual lookups happen only after the supported default context is restored.
            for (int pass = 0; pass != 2; ++pass)
                REQUIRE (sameBits (draw (*e, 32, 500), expected.pcm));
            const auto refilled = Access::find (*e, 261, 532);
            REQUIRE (refilled.has_value()); REQUIRE (refilled->state == Access::State::Complete);
            const auto replay = Access::select (*e, clock, 0, unlimited, 261, 532);
            REQUIRE (replay.replay); REQUIRE (Access::recordedControl (*e, replay.slot) == stable.control);
        }
    }

    SECTION ("captured same and different clocks bypass primed ordinary entries and restricted handoff")
    {
        for (const bool replace : { false, true })
        {
            auto capturedProject = clockProject (120.0); capturedProject.meterMap = { { 0, 3, 4 } };
            auto original = transport (capturedProject);
            std::shared_ptr<CapturedCountIn> session;
            REQUIRE (original->prepareCountIn (100, session) == TimeLookupStatus::Ok);
            const auto boundary = session->openingFrame(); REQUIRE (boundary > 240);
            auto currentProject = capturedProject;
            if (replace) { currentProject.tempoMap = { { 0, 90.0, TempoCurve::Jump } }; currentProject.meterMap = { { 0, 5, 8 } }; }
            auto replacement = replace ? transport (currentProject) : nullptr;
            auto& e = replace ? *replacement : *original;
            const auto ordinary = reference (*e.metronomeClock(), tables, boundary - 20, 64, zeros (64));
            const NumericControl steady;
            for (int pass = 0; pass != 3; ++pass) { steady.restore(); REQUIRE (sameBits (draw (e, 64, boundary - 20), ordinary.pcm)); }
            const auto primed = Access::entries (e);
            (void) e.setCountIn (session); REQUIRE (e.locate (boundary - 20));
            e.processBlock (nullptr, 0, 0); REQUIRE_FALSE (session->completed());
            REQUIRE (Access::entries (e) == primed);
            const auto before = reference (*original->metronomeClock(), tables, boundary - 20, 20, zeros (20), 0, boundary);
            REQUIRE (sameBits (publicRender (e, 20, 20), before.pcm)); REQUIRE (session->completed());
            REQUIRE (Access::entries (e) == primed);
            auto after = reference (*e.metronomeClock(), tables, boundary, 31, zeros (31), boundary).pcm;
            const auto tail = reference (*original->metronomeClock(), tables, boundary, 31, zeros (31), 0, boundary).pcm;
            for (std::size_t c = 0; c < 2; ++c) for (std::size_t n = 0; n < 31; ++n) after[c][n] += tail[c][n];
            REQUIRE (sameBits (publicRender (e, 31, 31), after)); REQUIRE (Access::entries (e) == primed);
            REQUIRE (e.locate (boundary - 20));
            REQUIRE (sameBits (publicRender (e, 64, 64), ordinary.pcm)); // Old one-shot tail cannot recur.
        }
    }

    SECTION ("a fresh worker first enters the real callback without thread-side warmup")
    {
        auto e = graphEngine (project, 128); // Control builds the real stereo source graph and cache storage.
        REQUIRE (e->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (e->setLoop (0, 128)); // Queued; no Control processBlock has run on this engine.
        const auto expected = reference (*e->metronomeClock(), tables, 0, 128, source (128));
        for (const auto& x : Access::entries (*e)) REQUIRE (x.state == Access::State::Empty);
        using Block = std::array<std::array<float, 128>, 2>;
        std::array<Block, 50> blocks {};
        std::array<std::array<float*, 2>, 50> pointers {};
        for (std::size_t call = 0; call < blocks.size(); ++call)
            for (std::size_t channel = 0; channel < 2; ++channel)
                pointers[call][channel] = blocks[call][channel].data();
        PlaybackEngine* const audio = e.get();
        std::thread worker ([audio, &pointers] {
            // First engine action and first user statement: no worker assertion/setup/math/warmup.
            audio->processBlock (pointers[0].data(), 2, 128);
            for (std::size_t call = 1; call < pointers.size(); ++call)
                audio->processBlock (pointers[call].data(), 2, 128);
        });
        worker.join();
        // Assertions and conversions are Control-only after the fresh worker has finished.
        for (const auto& block : blocks)
        {
            const Stereo actual { std::vector<float> (block[0].begin(), block[0].end()),
                                  std::vector<float> (block[1].begin(), block[1].end()) };
            REQUIRE (sameBits (actual, expected.pcm));
        }
        REQUIRE (e->metronomeLookupStatus() == TimeLookupStatus::Ok);
        if (Access::supported())
        {
            const auto complete = Access::find (*e, 0, 128);
            REQUIRE (complete.has_value()); REQUIRE (complete->state == Access::State::Complete);
            REQUIRE (complete->rows == 129);
            REQUIRE (Access::select (*e, e->metronomeClock().get(), 0, unlimited, 0, 128).replay);
        }
        else unsupportedFallback (*e, tables);
    }

    SECTION ("public seeks loop wraps partitions toggles shuttle stop and zero drains preserve PCM")
    {
        const auto oracleOwner = transport (project);
        const auto loop = reference (*oracleOwner->metronomeClock(), tables, 13, 298, zeros (298));
        auto expected = zeros (700);
        for (std::size_t c = 0; c < 2; ++c) for (std::size_t n = 0; n < 700; ++n) expected[c][n] = loop.pcm[c][n % 298];
        for (const int partition : { 1, 7, 128, 257 })
        {
            auto e = transport (project); REQUIRE (e->setLoop (13, 311)); REQUIRE (e->locate (13));
            REQUIRE (sameBits (publicRender (*e, 700, partition), expected));
        }
        auto e = transport (project);
        const auto sought = reference (*e->metronomeClock(), tables, 900, 32, zeros (32));
        REQUIRE (e->locate (900)); REQUIRE (e->setLoop (900, 932));
        const auto unchanged = Access::entries (*e);
        e->processBlock (nullptr, 0, 0); REQUIRE (e->playheadFrame() == 900);
        REQUIRE (Access::entries (*e) == unchanged);
        REQUIRE (sameBits (publicRender (*e, 32, 32), sought.pcm));
        REQUIRE (e->setMetronomeEnabled (false) == TimeLookupStatus::Ok);
        const auto disabled = Access::entries (*e);
        REQUIRE (sameBits (publicRender (*e, 32, 32), zeros (32))); REQUIRE (Access::entries (*e) == disabled);
        REQUIRE (e->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        auto shuttled = zeros (32);
        for (std::size_t c = 0; c < 2; ++c) for (std::size_t n = 0; n < 32; ++n)
            shuttled[c][n] = sought.pcm[c][(2 * n) % 32]; // Stride2 across two absolute32-Frame loop spans.
        REQUIRE_FALSE (sameBits (shuttled, zeros (32)));
        REQUIRE (e->setPlaybackRate (2));
        REQUIRE (sameBits (publicRender (*e, 32, 32), shuttled));
        const auto afterShuttle = Access::entries (*e); // Shuttle's real overlay may fill/replay the cache.
        REQUIRE (e->stop()); REQUIRE (sameBits (publicRender (*e, 32, 32), zeros (32)));
        REQUIRE (e->playheadFrame() == 900); REQUIRE (Access::entries (*e) == afterShuttle);
        REQUIRE (e->play()); REQUIRE (e->locate (900));
        REQUIRE (sameBits (publicRender (*e, 32, 32), sought.pcm));
    }
}
