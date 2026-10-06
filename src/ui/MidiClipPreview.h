// YES DAW — where the arrange view draws a MIDI Clip and the notes inside it (2026-10-06 repair).
//
// The engine plays a note at (clip start + note offset), converted by the CLIP's own time law
// (engine/Midi.h flattenMidiClipForProjection): a SampleLocked clip's ticks ARE frames; a TempoLocked
// clip's go through the tempo map. The preview used the tempo map for every clip, so at 48 kHz / 120 BPM
// the notes of every UI-made (SampleLocked) clip were drawn ~1.56x to the right of where they sound — past
// their own clip box for a clip that starts late. One law here, shared by the drawing and its gate.
#pragma once

#include "engine/Project.h"
#include "engine/Time.h"
#include "ui/TimelineCanvas.h"

#include <algorithm>
#include <vector>

namespace yesdaw::ui {

struct MidiClipPreview
{
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    std::vector<TimelineClipNote> notes;   // the first pass of a looped clip
};

[[nodiscard]] inline bool midiClipFrameAt (const engine::Project& project, const engine::MidiClip& clip,
                                           engine::Tick tick, double& frame) noexcept
{
    if (tick < 0)
        return false;
    if (clip.timeBase == engine::TimeBase::SampleLocked)
    {
        frame = static_cast<double> (tick);
        return true;
    }
    return engine::tickToFrame (engine::TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                                project.sampleRate, tick, frame);
}

[[nodiscard]] inline bool midiClipPreview (const engine::Project& project, const engine::MidiClip& clip, int clipId,
                                           MidiClipPreview& out)
{
    out = {};
    const double sampleRate = project.sampleRate.hz;
    double startFrame = 0.0, endFrame = 0.0;
    if (! project.sampleRate.isValid() || clip.timelineLength <= 0
        || ! midiClipFrameAt (project, clip, clip.timelineStart, startFrame)
        || ! midiClipFrameAt (project, clip, clip.timelineStart + clip.timelineLength, endFrame))
        return false;
    out.startSeconds = startFrame / sampleRate;
    out.lengthSeconds = std::max (0.0, (endFrame - startFrame) / sampleRate);
    for (const engine::Note& note : clip.notes)
    {
        double noteStart = 0.0, noteEnd = 0.0;
        if (! midiClipFrameAt (project, clip, clip.timelineStart + note.startTick, noteStart)
            || ! midiClipFrameAt (project, clip, clip.timelineStart + note.startTick + note.lengthTicks, noteEnd))
            continue;
        out.notes.push_back ({ clipId, noteStart / sampleRate, std::max (0.0, (noteEnd - noteStart) / sampleRate),
                               static_cast<int> (note.key) });
    }
    return true;
}

} // namespace yesdaw::ui
