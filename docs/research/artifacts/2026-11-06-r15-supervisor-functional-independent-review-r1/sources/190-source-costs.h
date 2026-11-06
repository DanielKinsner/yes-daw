#pragma once
// SOURCE-FIRST PROTOTYPE. No numeric price or binding definition is supplied.
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace clock_stage::source_costs {
inline constexpr std::uint64_t researchCap=1ull<<34;
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
// Declaration ONLY. There is no implementation/constant catalogue in this unit.
const Catalogue& immutableCatalogue();
class Catalogue final {
 const WholeBound whole_;
 const std::array<FamilyBound,familyCount> rows_;
 Catalogue(WholeBound,std::array<FamilyBound,familyCount>);
 friend const Catalogue& immutableCatalogue();
public:
 Catalogue(const Catalogue&)=delete;Catalogue& operator=(const Catalogue&)=delete;
 const WholeBound& whole()const noexcept{return whole_;}
 const FamilyBound& family(Family id)const {
  const auto index=static_cast<std::size_t>(id);
  if(index>=rows_.size()||rows_[index].family!=id)throw std::logic_error("UNBOUND_OR_INVALID_SOURCE_FAMILY_ID");
  const auto& bound=rows_[index];
  if(bound.upperUnits==0||bound.upperUnits>researchCap)throw std::logic_error("UNBOUND_OR_INVALID_SOURCE_FAMILY_PRICE");
  return bound;
 }
};
} // namespace clock_stage::source_costs
