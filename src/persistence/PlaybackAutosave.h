// YES DAW - H8 production autosave caller.
//
// The control loop owns the edit revision; persistence owns the bundle-shaped H6 snapshot. This tiny bridge
// keeps SQLite out of PlaybackEngine while giving autosave a real edit-tick caller.
//
// CONTROL THREAD ONLY: writeAutosaveSnapshot does heavy disk I/O (open SQLite, copy assets, fsync). NEVER
// call this from the audio callback ("tick" here means a control-loop edit/idle tick, not processBlock).

#pragma once

#include "engine/PlaybackEngine.h"
#include "persistence/AutosaveRecovery.h"

namespace yesdaw::persistence {

[[nodiscard]] inline AutosaveResult writeAutosaveFromControlTick (engine::PlaybackEngine& playback,
                                                                  const ProjectBundleDb& sourceDb,
                                                                  const engine::Project& project)
{
    if (! playback.needsAutosave())
        return autosave_detail::ok();

    AutosaveResult result = writeAutosaveSnapshot (sourceDb, project);
    if (result.ok())
        playback.markAutosaved();

    return result;
}

} // namespace yesdaw::persistence
