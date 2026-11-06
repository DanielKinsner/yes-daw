#include "../20261105-musical-funded-legacy-adapter-source-r5/legacy-adapter.h"

namespace yesdaw::r5_codegen_evidence {

// Compile-only external entry. No caller state, Data definition or initializer.
// This function is not an admitted production caller and must not be executed.
__declspec(noinline) app::musical_origin_source_r8::LegacyReadout emitWorker (
    app::musical_origin_source_r8::ControlRequestOwner& owner,
    const std::atomic<bool>& cancel) noexcept
{
    return app::musical_origin_source_r8::ApplicationRequests::workerBuildLegacy (owner, cancel);
}

} // namespace yesdaw::r5_codegen_evidence
