// Whole independent position-cache proof; SOURCE ONLY until root execution gates.
#include "engine/PlaybackEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace yesdaw::engine::click_cache_proof {
struct Access
{
    static bool supported() noexcept { return PlaybackEngine::kClickCacheContextSupported; }
    static std::size_t cells (const PlaybackEngine& e) noexcept { return e.clickPositionCacheCells_.size(); }
    static std::size_t requested (const PlaybackEngine& e) noexcept
    {
        return sizeof (e.clickCacheEntries_) + sizeof (e.clickCacheRows_)
            + sizeof (e.clickCacheRowsPerEntry_) + sizeof (e.clickCacheNextEntry_)
            + sizeof (e.clickPositionCacheCells_)
            + e.clickCacheRows_.capacity() * sizeof (e.clickCacheRows_[0])
            + e.clickPositionCacheCells_.capacity() * sizeof (e.clickPositionCacheCells_[0]);
    }
    static auto window (PlaybackEngine& e, const MusicalClickMap* clock,
        std::int64_t first, std::int64_t end, std::int64_t minimum = 0,
        std::int64_t limit = INT64_MAX) noexcept
    { return e.makeClickWindow (clock, first, end, minimum, limit); }
    static auto cellsSnapshot (const PlaybackEngine& e)
    {
        using Row = std::tuple<std::size_t, std::uint64_t, std::uint64_t,
                               std::uint64_t, std::uint64_t, bool, std::uint8_t>;
        std::vector<Row> out;
        for (const auto& cell : e.clickPositionCacheCells_)
            out.emplace_back (cell.segment, cell.ordinal, cell.control,
                std::bit_cast<std::uint64_t> (cell.value.low), std::bit_cast<std::uint64_t> (cell.value.high),
                cell.value.numericFailure, cell.kind);
        return out;
    }
    static std::size_t occupied (const PlaybackEngine& e, std::uint64_t control) noexcept
    {
        std::size_t result = 0;
        for (const auto& c : e.clickPositionCacheCells_) if (c.kind != 0 && c.control == control) ++result;
        return result;
    }
    static bool allResidentControls (const PlaybackEngine& e, std::uint64_t control) noexcept
    {
        bool observed = false;
        for (const auto& cell : e.clickPositionCacheCells_)
            if (cell.kind != 0)
            {
                observed = true;
                if (cell.control != control) return false;
            }
        return observed;
    }
};
} // namespace yesdaw::engine::click_cache_proof

namespace {
using namespace yesdaw::engine;
using Access = click_cache_proof::Access;
std::uint64_t nativeControl() noexcept
{
#if defined(__x86_64__) || defined(_M_X64)
    return static_cast<std::uint32_t> (_mm_getcsr()) & ~std::uint32_t { 0x3f };
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t value;
    __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (value));
    return value;
#else
    return 0;
#endif
}
auto fields (const MusicalClickLookup& q)
{
    return std::tuple { q.status,q.found,q.frame,q.downbeat,q.segmentComparisons,
                       q.ordinalProbes,q.roughProbes,q.tightProbes,q.precisionComparisons };
}
Project projectFor (bool coincident)
{
    Project p;
    p.sampleRate = { 48000.0 };
    p.meterMap = { { 0,65535,65535 } };
    p.tempoMap = { { 0,6000.0,TempoCurve::LinearRamp }, { 61440,12000.0,TempoCurve::Jump } };
    if (coincident)
    {
        p.meterMap.clear(); p.tempoMap.clear();
        for (Tick tick = 0; tick != 8192; ++tick)
        {
            p.meterMap.push_back ({ tick,static_cast<std::uint16_t> (tick%65535+1),65535 });
            p.tempoMap.push_back ({ tick,6000.0+6000.0*static_cast<double> (tick)/61440.0,TempoCurve::LinearRamp });
        }
        p.tempoMap.push_back ({ 61440,12000.0,TempoCurve::Jump });
    }
    return p;
}
std::unique_ptr<PlaybackEngine> engineFor (const Project& p, int maximum = 257)
{
    auto engine = PlaybackEngine::createTransportOnly (p, maximum);
    REQUIRE (engine != nullptr);
    REQUIRE (engine->metronomeClock() != nullptr);
    REQUIRE (engine->setLoop (256,1024));
    engine->processBlock (nullptr,0,0);
    REQUIRE (engine->loopEnabled());
    REQUIRE (Access::requested (*engine) <= 1024u*1024u);
    if (Access::supported()) REQUIRE (Access::cells (*engine) >= 64);
    else REQUIRE (Access::cells (*engine) == 0);
    return engine;
}
std::uint64_t exactWindow (PlaybackEngine& e, const MusicalClickMap* clock,
    std::int64_t first, std::int64_t end, std::int64_t minimum = 0, std::int64_t limit = INT64_MAX)
{
    auto original = clock->onsetsInRange (first,end);
    auto cached = Access::window (e,clock,first,end,minimum,limit);
    REQUIRE (fields (cached.preparation()) == fields (original.preparation()));
    const auto span = end >= first && first >= 0 && end-first <= 16384 ? end-first : 0;
    std::uint64_t found = 0;
    std::int64_t previous = -1;
    for (std::int64_t i = 0; i != span+2; ++i)
    {
        const auto a = original.next(), b = cached.next();
        REQUIRE (fields (b) == fields (a));
        if (a.status != TimeLookupStatus::Ok || ! a.found)
        {
            REQUIRE (fields (original.next()) == fields (cached.next()));
            return found;
        }
        REQUIRE (a.frame > previous);
        REQUIRE ((a.frame >= first && a.frame < end));
        previous = a.frame; ++found;
    }
    FAIL ("Actual Window failed its distinct native Frame bound");
    return 0;
}
struct NumericRestore
{
#if defined(__x86_64__) || defined(_M_X64)
    std::uint32_t original = _mm_getcsr();
    ~NumericRestore() { _mm_setcsr (original); }
    void nearest (unsigned flush) const noexcept
    {
        auto next = (original | std::uint32_t { 0x1f80 }) & ~std::uint32_t { 0x6000 | 0x8040 | 0x3f };
        if ((flush & 1u) != 0) next |= 0x8000;
        if ((flush & 2u) != 0) next |= 0x0040;
        _mm_setcsr (next);
    }
    void unsupported() const noexcept { _mm_setcsr ((original | 0x1f80u | 0x2000u) & ~0x4000u); }
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t original = [] { std::uint64_t v; __asm__ __volatile__ ("mrs %0, fpcr" : "=r" (v)); return v; }();
    ~NumericRestore() { __asm__ __volatile__ ("msr fpcr, %0" : : "r" (original)); }
    void nearest (unsigned flush) const noexcept
    {
        auto next = original & ~((UINT64_C (3)<<22)|(UINT64_C (0x1f)<<8)|(UINT64_C (1)<<15)
                                |(UINT64_C (1)<<24)|(UINT64_C (1)<<19));
        if ((flush & 1u) != 0) next |= UINT64_C (1)<<24;
        if ((flush & 2u) != 0) next |= UINT64_C (1)<<19;
        __asm__ __volatile__ ("msr fpcr, %0" : : "r" (next));
    }
    void unsupported() const noexcept
    {
        const auto next = (original & ~(UINT64_C (3)<<22)) | (UINT64_C (1)<<22);
        __asm__ __volatile__ ("msr fpcr, %0" : : "r" (next));
    }
#else
    void nearest (unsigned) const noexcept {}
    void unsupported() const noexcept {}
#endif
};
} // namespace

TEST_CASE ("overlapping metronome windows reuse exact positions without changing lookup receipts",
           "[playback][musical-click][position-cache]")
{
    NumericRestore numeric;
    numeric.nearest (false);
    SECTION ("all loop origins and partitions preserve each actual search receipt cold and warm")
    {
        for (const bool coincident : { false,true })
        {
            auto e = engineFor (projectFor (coincident));
            for (const int block : { 128,257 })
                for (std::int64_t start = 256; start != 1024; ++start)
                {
                    const auto firstCount = std::min<std::int64_t> (block,1024-start);
                    exactWindow (*e,e->metronomeClock().get(),start-239,start+firstCount);
                    if (firstCount != block)
                        exactWindow (*e,e->metronomeClock().get(),17,256+block-firstCount);
                }
            if (Access::supported()) REQUIRE (Access::occupied (*e,nativeControl()) != 0);
        }
    }
    SECTION ("real preparation capacity yields a small collision table without changing semantics")
    {
        auto e = engineFor (projectFor (true),7900);
        if (Access::supported()) REQUIRE (Access::cells (*e) <= 128);
        for (int pass = 0; pass != 2; ++pass)
            for (std::int64_t start = 256; start < 1024; start += 7)
                exactWindow (*e,e->metronomeClock().get(),start-239,std::min<std::int64_t> (1024,start+257));
        REQUIRE (Access::requested (*e) <= 1024u*1024u);
    }
    SECTION ("equal ordinals across distinct Meter segments retain their own exact positions")
    {
        Project p; p.sampleRate = { 48000.0 };
        p.tempoMap = { { 0,125.0,TempoCurve::Jump }, { 1200,187.0,TempoCurve::LinearRamp },
                       { 2400,93.0,TempoCurve::Jump } };
        p.meterMap = { { 0,7,65535 }, { 480,5,32768 }, { 960,3,16384 }, { 1920,11,65535 } };
        auto e = engineFor (p,7900);
        for (int pass = 0; pass != 2; ++pass)
            for (std::int64_t first = 0; first < 5000; first += 97)
                exactWindow (*e,e->metronomeClock().get(),first,first+496);
    }
    SECTION ("empty successful prefix failures and native numeric boundaries retain terminal status")
    {
        for (const double bpm : { 125.0,1.0e-300,1.0e300,std::numeric_limits<double>::denorm_min() })
        {
            Project p; p.sampleRate = { 48000.0 }; p.tempoMap = { { 0,bpm,TempoCurve::Jump } };
            p.meterMap = { { 0,7,65535 } };
            auto e = engineFor (p);
            for (int pass = 0; pass != 3; ++pass)
                for (const auto [first,end] : std::array<std::pair<std::int64_t,std::int64_t>,8> {{
                    {-1,8},{8,7},{8,8},{0,1},{1,17},{17,384},
                    {(INT64_C (1)<<53)+1,(INT64_C (1)<<53)+33},{INT64_MAX-32,INT64_MAX} }})
                    exactWindow (*e,e->metronomeClock().get(),first,end);
        }
    }
    SECTION ("full numeric control keys change and unsupported modes bypass every position cell")
    {
        Project p; p.sampleRate = { 48000.0 };
        p.tempoMap = { { 0,125.0,TempoCurve::Jump } }; p.meterMap = { { 0,7,65535 } };
        auto e = engineFor (p);
        if (! Access::supported()) return;
        auto footprint = Access::cellsSnapshot (*e);
        bool first = true;
        for (const unsigned flush : { 0u,1u,0u,2u,0u,3u,0u })
        {
            numeric.nearest (flush);
            const auto context = nativeControl(); // Independent native read, no production key helper.
            exactWindow (*e,e->metronomeClock().get(),17,65);
            REQUIRE (Access::allResidentControls (*e,context));
            auto actual = Access::cellsSnapshot (*e);
            for (auto& row : actual) std::get<2> (row) = 0;
            if (! first) REQUIRE (actual == footprint);
            footprint = std::move (actual); first = false;
        }
        const auto before = Access::cellsSnapshot (*e);
        numeric.unsupported();
        exactWindow (*e,e->metronomeClock().get(),17,513);
        REQUIRE (Access::cellsSnapshot (*e) == before);
    }
    SECTION ("other clocks restricted count-in and disabled loops leave the owned table untouched")
    {
        const auto p = projectFor (false);
        auto e = engineFor (p);
        exactWindow (*e,e->metronomeClock().get(),17,513);
        const auto before = Access::cellsSnapshot (*e);
        MusicalClickMap other;
        REQUIRE (MusicalClickMap::build ({p.tempoMap.data(),p.tempoMap.size()},
            {p.meterMap.data(),p.meterMap.size()},p.sampleRate,other) == TimeLookupStatus::Ok);
        exactWindow (*e,&other,17,513);
        exactWindow (*e,e->metronomeClock().get(),17,513,1);
        exactWindow (*e,e->metronomeClock().get(),17,513,0,600);
        REQUIRE (Access::cellsSnapshot (*e) == before);
        REQUIRE (e->clearLoop()); e->processBlock (nullptr,0,0);
        exactWindow (*e,e->metronomeClock().get(),17,513);
        REQUIRE (Access::cellsSnapshot (*e) == before);
    }
}
