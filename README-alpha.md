# YES DAW — Alpha (portable build)

This is a **portable, unsigned alpha** of YES DAW. There is no installer: unzip it anywhere and
run the executable. It writes nothing to system locations you didn't point it at.

> Alpha means *early*. Expect rough edges, and expect to lose work if you lean on it. Don't make
> anything you can't afford to redo yet.

## Run it

**Windows**
1. Unzip `YesDaw-<version>-win64-portable.zip` to a folder you own (e.g. `Desktop\YesDaw`).
2. Double-click `YesDaw.exe`.
3. Windows SmartScreen will warn that it's from an unidentified publisher — that's expected for an
   unsigned alpha (code signing is a beta item, ADR-0037). Choose **More info → Run anyway**.

**macOS** (produced but not notarized yet — beta item)
1. Unzip and move `YesDaw.app` to `/Applications` (or anywhere you like).
2. First launch: right-click the app → **Open** → **Open** to get past Gatekeeper for the
   unsigned build.

## What's in the zip

| File | What it is |
|---|---|
| `YesDaw.exe` / `YesDaw.app` | the application |
| `YesDawSelfCheck.exe` | headless project self-check CLI |
| `verify-hardware.ps1` | the one-command hardware verifier (below) |
| `YesDawHardwarePlaybackCheck.exe` | packaged playback stage checker (used by the verifier) |
| `YesDawHardwareRecordingCheck.exe` | packaged recording stage checker (used by the verifier) |
| `YesDawFrameCheck.exe` | packaged headless frame stage checker (used by the verifier) |
| `package-manifest.json` | integrity manifest the verifier validates before touching hardware |
| `verify-fixtures/` | verdict-policy fixtures for `verify-hardware.ps1 -SelfTest` |
| `version.txt` | the exact build version (git-describe); the app's `--version` matches this |
| `README-alpha.md` | this file |
| `LICENSE` | present once a license is chosen (alpha zips may ship without one) |

## Verify this machine's hardware (one command, no judgment calls)

Open PowerShell **in the unzipped folder** (tip: keep the folder path short, e.g. `C:\YesDaw`) and
run:

```
powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1
```

It validates the package against `package-manifest.json`, then runs packaged playback, recording,
and headless frame checks and prints one PASS / FAIL / setup-incomplete verdict per stage plus an
overall exit code (0 = all pass, 1 = a measured gate failed, 2 = incomplete). **Playback and
recording will make a short, quiet sound** — that is informational; nothing asks you to listen or
judge. Full evidence (JSON + proposed reality-lane rows) is retained under `hardware-results\`.
`-SelfTest` exercises the verdict policy and package negative controls without touching hardware.

## Verifying the build

`version.txt` is the source of truth for which build you have. Once the `--selfcheck` mode lands,
`YesDaw --selfcheck <bundle>` will headlessly load a project, render, export, and print a single
`PASS`/`FAIL` line — so you can confirm a build is sound without opening the UI.

## Reporting issues

Note the `version.txt` value and what you did leading up to the problem. Reproducible steps beat
"it felt off" — but if something felt wrong, say that too; the feel session is a real check.
