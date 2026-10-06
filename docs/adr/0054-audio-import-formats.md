# 0054. Audio import formats: one decoder for WAV, AIFF, FLAC, Ogg Vorbis and MP3; drops that land as one edit

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose findings are folded in: an MP3's length is JUCE's estimate, so the Asset's frame count is
  defined as the pinned decoder's reported length and pinned across platforms by fixtures; reopen's integrity
  authority is the Asset row's frames / rate / channels, the reader only a decoder; bringing a drop's new
  tracks inside its undo is stated as the policy change it is; mixed MIDI-and-audio drops, partial drops,
  AIFF-C compression, 8-bit and 64-bit WAV are decided; the MP3 licence sentence is corrected. Committed alone,
  before any G5.1 code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.1 ("Import formats and cross-rate audio. JUCE formats: WAV/AIFF/FLAC/OGG, MP3 where
  licensed; drop at the pointer on a lane, multiple files on consecutive tracks, undoable. ADR-0010 remains
  authoritative … Implement format decoding and then cross-rate playback/export as separate checkpoints.") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  **ADR-0010** (one project rate; an Asset keeps its original, content-hashed bytes; resample at read);
  ADR-0011 (Asset / Clip identity); ADR-0012 (the bundle's asset store and its atomic copy); ADR-0036 (recorded
  takes share the asset store); ADR-0042 (mono and stereo sources only); ADR-0047 / G3.7 (MIDI file import).

## Context

Today the app reads one format. `decodeProjectWav` opens a file with JUCE's WAV reader and returns nothing —
no reason — for anything else; the status line can only say "WAV only, stereo max". The chooser offers
`.wav;.wave`, the timeline's drop filter `.wav` alone. A drop of several files lands each on its own undo
step and silently stops at the last existing track. The bundle already stores an import's **original bytes**
(`audio/<sha256>.asset`, the hash of exactly those bytes) and reopen decodes them again and checks the
decode's frames, rate and channels against the Asset row, so the asset store is format-neutral already; only
the decoder and the import verbs are WAV-shaped. A file at another sample rate is refused with the rate fact
("no resampling yet"); that refusal is the cross-rate checkpoint's to lift, not this one's.

JUCE 8.0.4 (pinned) ships readers for WAV, AIFF, FLAC and Ogg Vorbis (on by default) and an MP3 decoder behind
`JUCE_USE_MP3AUDIOFORMAT` (off by default). Its MP3 reader reports a length read from a Xing / LAME / VBRI
header when there is one and otherwise **estimates** it from the stream size and the first frame's size
(`juce_MP3AudioFormat.cpp`, `findLength`). Its Ogg reader holds the stream length in 32 bits. JUCE also offers
the platform's own decoders (Windows Media Foundation, Core Audio).

## Options considered

1. **One app decoder over JUCE's own readers, a fixed format list (chosen).** The same code decodes every
   format on every platform and in CI, so an Asset's reported length is the same everywhere for a given file
   and JUCE version.
2. **Platform decoders (Media Foundation / Core Audio).** More formats for free (AAC, WMA, ALAC), but the
   decoded length and samples depend on the operating system and its codec updates: an Asset saved on one
   machine could fail its frame check on another. Rejected.
3. **Transcode on import and store the float WAV.** Simplest reopen, but it replaces the user's bytes with a
   derived copy — against ADR-0010 and the plan's "original, content-hashed asset bytes". Rejected.

**MP3:** enable JUCE's own decoder (`JUCE_USE_MP3AUDIOFORMAT=1`) rather than leave MP3 out. The patents on
MPEG-1/2 Layer III expired in 2017; the JUCE MP3 code is covered by the same JUCE 8 licence (EULA or AGPLv3)
as the rest of the framework the project already links, and no encoder is added. JUCE's own disclaimer on
that file (it is "not guaranteed to be free from infringements of 3rd-party intellectual property") is noted;
the distribution-licence review of the shipped binary stays with G8.

## Decision

**One decoder.** A single app decoder owns the supported list, in this order: **WAV** (`.wav`, `.wave`),
**AIFF** (`.aif`, `.aiff`, `.aifc`), **FLAC** (`.flac`), **Ogg Vorbis** (`.ogg`, `.oga`), **MP3** (`.mp3`).
The import chooser's filter, the timeline's drop filter, the Sampler pad's drop and file chooser, and reopen
all use this one list — no surface accepts a format another refuses — and all of them report the same
refusal reasons.

**Import picks the reader by extension** (case-insensitive). The decoder returns either the decoded audio
or a **reason**, and every refusal names its file and that reason:
- an extension not on the list: "unsupported format (.xyz)";
- a listed extension whose bytes that reader cannot open: "not a readable FLAC file" (the format's name) —
  this covers WAV encodings the reader does not take (64-bit float, compressed WAV formats); 8-, 16-, 24- and
  32-bit integer and 32-bit float WAV are supported;
- an AIFF-C whose compression type the reader does not take (anything but uncompressed, `sowt` and `fl32`):
  "AIFF compression 'ulaw' is not supported" (the type read from the file's `COMM` chunk);
- more than two channels: "6 channels (mono or stereo only)" (ADR-0042);
- no frames: "no audio in the file"; more than 2^31 − 1 frames: "too long to import";
- a read that fails part-way: "could not be read to the end".

A refused file changes nothing: no asset bytes are kept and the project is untouched.

**The stored bytes are the original file's**, copied verbatim into the asset store and hashed (SHA-256) as
they are today; the Asset row and the schema (v34) are unchanged.

**An Asset's frame count is the pinned decoder's reported length** for those bytes — exact for WAV, AIFF,
FLAC and Ogg; for MP3 the header's count when there is a Xing / LAME / VBRI header, otherwise JUCE's estimate
from the stream size. It is a property of the bytes and JUCE 8.0.4, not of the file in general: the same bytes
give the same length on every platform (gated below). The decode is that many frames: past the stream's real
end it is silence; an estimate short of the real end drops the tail. Samples are float, interleaved, one or
two channels; lossy formats keep any encoder delay and padding exactly as the decoder yields them (no gapless
trimming here — a later ADR may add it), so an MP3 or Ogg made from a WAV does not line up sample-exact with
that WAV. Lossy decodes may differ across platforms in the last bits of a sample (float codec maths); the
length never does.

**Reopen: the Asset row is the authority, the reader only a decoder.** A stored asset is identified by its
content: reopen tries the readers in the list's order — WAV, AIFF, FLAC and Ogg recognise their own headers;
MP3, the one format without a fixed signature, is tried last — and the **first reader whose decode has the
Asset's frames, rate and channels** is used. If none does, the asset is reported missing or corrupt (today's
law and message).

**Rate.** Unchanged in this checkpoint: a file at another rate is refused with the rate fact. The cross-rate
checkpoint gets its own ADR under ADR-0010 before any resampling code.

**Drops land as one edit.** A drop of supported audio files lands them on **consecutive tracks**, starting at
the lane under the pointer, at the pointer's (snapped) time; files past the last track **create new audio
tracks** for themselves; a drop on the empty area below the tracks starts at a new track. The whole drop —
its new tracks and its clips — is **one undo step**. This is a change of policy: until now an import's
auto-created default track stayed outside the undo; a drop's tracks are now part of the edit that needed
them, so undo removes them and redo puts them back with the same ids (the undo stack replays its recorded
commands). The asset rows and bundle bytes stay outside the undo, as today (inert without a clip; the hash
dedupes a redo). A drop with some refused files lands the accepted ones — one undo step holding exactly
those files and the tracks they created — and names every refused file with its reason; a drop in which
every file is refused changes nothing. **MIDI files** (`.mid`, `.midi`) in a drop keep G3.7's import on
their own lane, each its own undo step, placed after the audio files' lanes; their refusals are named on the
same status line.

**Names.** The action's label becomes "Import Audio" (its stable id and chord unchanged) and the chooser's
title "Import Audio"; the status line's refusal names each file and its reason instead of "WAV only".

## Consequences

- **Positive:** the common studio formats import, reopen and play the same on every platform; every refusal
  says why; a multi-file drop is one gesture to undo; the asset store and schema need no change.
- **Negative / accepted costs:** MP3 and Ogg decode their whole file into memory at import and on every
  reopen (as WAV does today) — a session of many long MP3s reopens slowly until G5.4 shares decoded buffers;
  MP3/Ogg clips start with the codec's own delay until a gapless decision is made; an MP3 without a length
  header may lose or pad its last fraction of a frame to JUCE's estimate; Ogg streams longer than 2^32 frames
  (about 24.8 h at 48 kHz) are outside support (the reader's length is 32-bit); AAC/ALAC/WMA stay unsupported
  (named as unsupported, not misread).
- **Gates (`[import-formats]`):** for each lossless format and depth (WAV 8/16/24/32-int and 32-float, AIFF
  16/24, FLAC 16/24) the decoded samples equal the source PCM; Ogg Vorbis (encoded in the test with JUCE's
  writer) decodes to the source's rate and channels and matches it within a stated signal-to-noise floor after
  alignment; committed MP3 fixtures made with ffmpeg (CBR with and without an Info header, VBR with a Xing
  header, an ID3v1-trailed file; the commands recorded beside them) each report a pinned length on every CI
  platform and match the source within a stated floor after alignment; decoding the same bytes twice is
  bit-identical; the stored `.asset` hash equals the source file's for every format, and reopen yields the
  same decode; each refusal above is produced with its reason and an unchanged project; reopen picks the
  reader by content and refuses an asset whose bytes no longer match its row; a three-file drop past the last
  track creates the missing tracks, lands the clips on consecutive lanes at the drop time, one undo removes
  clips and tracks, and redo restores them with the same ids.
- **Follow-ups:** the cross-rate ADR (G5.1 cp2); `CONTEXT.md`'s **Asset** entry names the supported formats,
  that the original bytes are kept whatever the format, and that an Asset's frames are the pinned decoder's
  length for its bytes.
