# How to run YES DAW (dogfood edition)

## Launch it

One command, from the repo folder, in PowerShell:

```
powershell -ExecutionPolicy Bypass -File tools\run-yesdaw.ps1
```

It builds the latest code and opens the app. The first run after a fresh pull can take a few
minutes; after that it's quick. When it works you'll see a line like:

```
[run] PASS: YesDaw launched (pid 12345, version a1b2c3d)
```

**If anything looks broken or stale:** close the app, run the script again, and tell me the
`version:` value it printed. That one code is how we know exactly which build you were looking
at — it kills the "was I even running the new version?" question before it starts.

## Make a project and pull in some music

1. **File → New Project** (or the New button). Pick where to save the `.yesdaw` project file.
2. **Import audio** (the Import button) — pick a WAV. It lands on the selected track at the
   playhead.
3. **More tracks:** Ctrl+T adds a track. Click a track's row on the left rail to select it, then
   import again to put a stem there.

## Things that work (try them)

- **Move** a clip: drag it. **Copy**: Alt+drag. **Split**: put the playhead where you want the
  cut (click the ruler) and press **B**.
- **Fades**: select a clip, use the Fade In / Fade Out sliders in the right panel — the curve
  draws on the clip itself.
- **Zoom**: Ctrl+mouse-wheel, or the `[-] 1.0x [+]` control in the toolbar. Ctrl+0 fits the
  whole project.
- **Mixer**: the bottom strip is always there (the Mixer Dock button hides it); the full mixer
  is the Mixer view. Faders, pan, mute/solo, sends, FX inserts all work and all undo (Ctrl+Z).
- **Play**: Space. Stop: K or Space again. Loop: drag on the ruler with Shift.

## Where to gripe

Open `docs/dogfood/2026-08-24-dan-session-1.md` and add a bullet for EVERY moment of friction,
no matter how small — "this felt slow", "I expected X to do Y", "I couldn't find Z". Those
bullets are literally the next work backlog, so petty complaints are the most valuable thing
you can produce in this session.
