#pragma once
// SOURCE-FIRST R5 under the separate research contract. All numeric data unbound.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace clock_stage::source_costs {
// Cross-language representation domain only. This is NOT a selected work limit.
inline constexpr std::uint64_t integerDomainMax=static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
enum class Family : std::size_t {
 CatalogueLookup, CheckedComposition, AdmissionAndQuote,
 ChildFixedProduce, ChildFixedVerify,
 QcBuild, ProductionBuild, OriginalFraction, IndependentFraction, FractionCache,
 QcAppendConstant, QcAppendRamp, ProductionAppendConstant, ProductionAppendRamp,
 IteratorVectorBeginEnd, IteratorUnwrap, PointerDistance, PointerNextAdvance,
 PointerNextIter, IteratorSeek, IteratorCallerReturn,
 UpperTickInterior, UpperTickEndpoint, UpperPrefixInterior, UpperControl,
 QcWholeConstant, QcWholeRamp, QcWholeEndpoint,
 QcNativeConstant, QcNativeRamp, QcNativePrefix,
 ProductionForwardConstant, ProductionForwardRamp,
 ProductionForwardZeroConstant, ProductionForwardZeroRamp,
 ProductionForwardEndpoint, ProductionForwardInverse,
 QcInverseNative, QcForwardNative,
 ProductionInverseNative,
 PortCacheRefill, CursorResetTest, CursorAdvance,
 PortFractionConstant, PortFractionRamp, PortZero,
 RationalCut, DenseOuter, InverseProduceOuter, InverseVerifyOuter,
 WordWriteDispatch, WordWriteFast, WordWriteFallback,
 WordReadDispatch, WordReadFast, WordReadFallback,
 StreamConstructionAndRetirement, StreamZeroInitialization,
 PutChunk, ExactChunk, FullRefill, Flush, FinishEOFAndClose,
 TextZeroInitialization, TextAppend, TextDigits, TextQuoted, TextWitness,
 MetadataNormal, MetadataInverse, WindowGrid, Picks,
 NormalControlsProduce, NormalControlsVerify,
 FailureRetirement, CleanupCloseAndFooter,
 Count
};
inline constexpr std::size_t familyCount=static_cast<std::size_t>(Family::Count);
// No default member initializers: these are bounds to be independently bound,
// not zero/default prices. Child fixed rows must cover every disjoint fixed
// helper/initialization/control/failure family, including quote instrumentation.
struct FamilyBound final { Family family; std::uint64_t upperUnits; };
struct WholeBound final {
 std::uint64_t runnerUpper,producerUpper,verifierUpper,totalUpper;
};
class Catalogue;
// Accessor is defined below. The reviewed_ static data has NO value definition.
const Catalogue& immutableCatalogue() noexcept;
class Catalogue final {
 const WholeBound whole_;
 const std::array<FamilyBound,familyCount> rows_;
 constexpr Catalogue(WholeBound whole,std::array<FamilyBound,familyCount> rows) noexcept
  :whole_(whole),rows_(rows){}
 static const Catalogue reviewed_;
 friend const Catalogue& immutableCatalogue() noexcept;
public:
 Catalogue(const Catalogue&)=delete;Catalogue& operator=(const Catalogue&)=delete;
 const WholeBound& whole()const noexcept{return whole_;}
 const FamilyBound& family(Family id)const {
  const auto index=static_cast<std::size_t>(id);
  if(index>=rows_.size()||rows_[index].family!=id)throw std::logic_error("UNBOUND_OR_INVALID_SOURCE_FAMILY_ID");
  const auto& bound=rows_[index];
  if(bound.upperUnits==0||bound.upperUnits>integerDomainMax)throw std::logic_error("UNBOUND_OR_INVALID_SOURCE_FAMILY_PRICE");
  return bound;
 }
};
inline const Catalogue& immutableCatalogue() noexcept{return Catalogue::reviewed_;}
// Later reviewed binding MUST define constinit const Catalogue Catalogue::reviewed_
// from derived literals, with no runtime factory/function-local guard/allocation.
// No static data/value definition or numeric price is provided by this prototype.
} // namespace clock_stage::source_costs
