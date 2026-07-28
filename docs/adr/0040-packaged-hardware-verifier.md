# 0040. Packaged, one-command hardware verification

- **Status:** Accepted
- **Date:** 2026-07-28
- **Deciders:** Dan + Codex
- **Related:** ADR-0005 (mechanical verification), ADR-0018 (recording latency), ADR-0035 (recording and device UX), ADR-0037 (alpha and the Reality lane), [`docs/reality-lane.md`](../reality-lane.md), [`docs/plans/2026-07-28-h17-packaged-hardware-verifier-plan.md`](../plans/2026-07-28-h17-packaged-hardware-verifier-plan.md)

## Context

The Reality lane is already defined as mechanical, but its current commands still assume a source checkout, CMake, and build-tree executables. The H17 package contains the app and device-free self-check only. The recording smoke is not built.

Dan does not use the development environment and should not need to understand the app UI to prove the packaged build on real hardware. H17 therefore needs a package-level verification surface, not instructions for running repository tools.

The locked playback gate remains 48 kHz, a requested and granted 128-frame Block, zero Underruns, and no callback-budget breach. The current Windows device grants 480 frames through shared WASAPI and 144 through exclusive WASAPI, so neither result can earn the 128-frame PASS. JUCE 8.0.4 can expose ASIO only when the build enables it and supplies the Steinberg ASIO SDK under its licence terms.

## Options considered

1. **Keep separate repository scripts.**
   - Pros: smallest code change; reuses the current wrappers.
   - Cons: requires a checkout, build tree, and developer commands; does not test the portable package Dan will use.
2. **Ship one package-root verifier that orchestrates packaged console checks.** *(chosen)*
   - Pros: one command, no UI knowledge, no developer setup, exact packaged-binary identity, and one machine-readable verdict.
   - Cons: adds package contents and a stable evidence contract that must be maintained.
3. **Automate the GUI.**
   - Pros: exercises the visible app.
   - Cons: couples the hardware gate to focus, window state, and visual automation without improving the audio evidence.

## Decision

The Windows portable package will include one root-level `verify-hardware.ps1` command. With no arguments it will select the default available hardware automatically and run the packaged playback, recording, and frame checks. It must not search a repository or build tree and must not invoke CMake.

The command will:

- verify that every invoked executable belongs to the same package version before granting credit;
- enumerate compiled audio backends and devices, record every attempted mode, and choose a deterministic low-latency route without opening a device picker;
- prefer a usable ASIO route on Windows when the package was legally built with ASIO support, then try supported WASAPI low-latency modes;
- keep 48 kHz / 128 frames as a hard playback requirement and fail rather than relabel a larger granted Block;
- run real Project playback, a recording capture/persistence/format check, and the packaged dense-Timeline frame check;
- distinguish a full recording round trip with loopback alignment from the explicitly weaker capture-only result permitted by the existing Reality-lane contract;
- write one structured result artifact plus a short console summary, and return `0` only when every required stage passes, `1` for a measured gate failure, or `2` for unsupported or incomplete setup;
- generate the proposed Reality-lane log row from measured data. An agent may commit that generated evidence but may not invent or manually upgrade a PASS.

The verifier will remain a console workflow. It will not ask Dan to listen, watch a meter, inspect a frame, operate the app UI, or run a development server.

ASIO support is the selected route for the observed Windows hardware because WASAPI cannot currently meet the locked target. This decision does not accept Steinberg's licence terms on Dan's behalf. ASIO implementation is blocked until Dan explicitly accepts those terms and the build/redistribution path is documented.

## Consequences

- **Positive:** the H17 Reality-lane proof becomes usable by the owner and applies to the exact portable artifact.
- **Positive:** one result records playback, recording, frame, device, backend, package identity, and any degradation without transcription.
- **Positive:** unsupported hardware produces an honest setup failure with evidence instead of an ambiguous developer error.
- **Negative:** the package must carry additional console verification code and keep its evidence schema compatible.
- **Negative:** a full recording-alignment proof still requires device loopback or a physical loopback cable; automation cannot manufacture that hardware path.
- **Open blocker:** Dan must explicitly accept the Steinberg ASIO SDK licence terms before ASIO code or SDK material is added to the build.
