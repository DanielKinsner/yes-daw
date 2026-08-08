# 0041. Empty Project transport

- **Status:** Accepted
- **Date:** 2026-08-08
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0022, ADR-0023, ADR-0033, H12 operable-session plan.

## Context

The shipped app disabled transport until a Project contained renderable media. That made a newly-created
Project behave like a mockup: Play could not start, the playhead could not advance, and the ruler could not
locate. A real DAW transport must exist before media does. Dan explicitly rejected the disabled empty-session
behavior on 2026-08-08.

## Options considered

1. Give every loaded Project a real transport that can render silence. This preserves the absolute-frame
   model and makes empty and populated Projects use the same audio callback. Accepted.
2. Insert hidden silent media so the existing graph builder reports a non-empty timeline. Rejected because
   hidden media is fake Project state and would contaminate editing, save, and export behavior.
3. Keep empty-session transport disabled. Rejected because it prevents basic DAW operation.

## Decision

Every loaded Project owns a `PlaybackEngine`. For a Project with no renderable media, the engine owns the
normal bounded transport command queue and an empty `Runtime`; the device callback renders zeros and advances
the same absolute-frame playhead used by populated Projects. No hidden Asset, Clip, Track, or graph node is
created. Offline export of an empty Project remains `EmptyTimeline`.

The UI polls a lock-free published transport snapshot on its control timer. Timeline ruler click and drag post
sample-frame locate commands through the existing transport queue. Meter and loudness surfaces remain invalid
until measurements from actual rendered audio are available.

## Verification

- New and reopened empty Projects have enabled Play/Stop/Locate controls.
- Playing an empty Project produces exact digital silence and advances the playhead by the processed frames.
- Stopping freezes the published playhead.
- Ruler click and drag locate the transport to the mechanically expected frame.
- No-Project startup exposes zero fake Tracks, Clips, mixer strips, meters, loudness, or Notes.
