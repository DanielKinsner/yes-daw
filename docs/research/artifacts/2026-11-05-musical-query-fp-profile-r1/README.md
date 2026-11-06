# Finite musical Clock floating-point measurement

Both native Windows console profiles passed on 5 November 2026 (local time).
The probe used MSVC 14.44.35207 x64, actual project Release flags and no LTO.
The two numerical translation units used the project default precise mode in one
executable and strict mode in the other. The read-only observer used strict mode
in both. Every compile, link and execution returned zero.

Each profile attempted 96 maps at six sample rates: 576 builds, 546 successful
Clocks, 18 genuine invalid-map refusals and 12 genuine numerical refusals. All
22,284 public integer queries matched an independent scalar replay using the
unchanged old append helper: 21,010 exact successful outputs, 546 invalid-input,
54 numerical-domain and 674 overflow results. The replay made 123,150 extra helper
calls. Of the row knots, 4,470 exposed matching successful double bytes; six
provided status/reset evidence only, with no private cached-byte credit.

This is finite measurement. It does not prove all-input prefix or native-path
equivalence, controls inside an allocation/callback interval, private overflowed
prefix bytes, general ABI behavior, operation prices, resource fit, a funded
recipe, DSP quality, model/jobs, Ready activation or another platform.
The observer saw four status-changing intervals and zero errno-changing intervals
per profile. Sticky flags were not cleared; the observations include the observer
and cannot isolate a cause or prove that an unchanged interval raised no flags.

The whole six-file probe froze at the first successful run, including every
helper, comment and its original README. `probe-authoring-README.md` is that exact
README; its original unrun statements describe the authoring boundary. This file
records the later actual runs. Source copies are byte-exact. Text logs have LF
copies; `archive-manifest.json` also stores their original bytes as base64 and
original SHA-256. No executables or objects are committed.

`run-receipt.json` records exact compiler arguments, binary identity, executable
hashes and raw exits. `whole-source-safety-review.json` records the independent
pre-execution review. `first-green-whole-source-freeze.json` pins the first green
boundary. `source-pins.json` pins the original source names and 24 unchanged local
headers; it does not pin the whole preprocessed SDK/toolchain closure.

To reproduce, stage the six authored sources under their original names (rename
`probe-authoring-README.md` to `README.md`), verify every pin against the repository,
and use the exact separate-TU compile/link commands in the receipt with paths
adjusted to the staging directory. Keep the observer strict in both builds,
NOINLINE bridges, no LTO and a verified AMD64 CONSOLE executable. Run headlessly
with redirected output, a bounded timeout and per-process error-box suppression.
The archived runner records the original procedure and checkout-specific paths;
its original helper is `build-ci/20261105-click-position-cache-integration-r1/console-helpers.ps1`.
A successful reproduction receives the same finite scope, never broader credit.
