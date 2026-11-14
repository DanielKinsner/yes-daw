# Clock R22 controller child invocation local work

This bounds the exact R22 `Invoke-TaskChild` body using the
[reviewed controller source convention](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/182-argument.md).
The table includes its authored branches, primitive source call
boundaries, result/handler destinations and nested script call
arguments. Nested controller bodies remain separate.

## Before and during the helper watch

| Local region | Actions |
| --- | ---: |
| Entry, finite child names/index, argument-count selection, quarantine check, fixed locals and cleanup list | 168 |
| Per supplied argument string loop visit and guard dispatch | 20 |
| Binding, spend, digest creation and target hash result, environment and request call sites | 260 |
| ProcessStartInfo constructor and properties, fixed key array, environment clear and argument-list add | 294 |
| Each of eight environment store groups | 52 |
| Process creation, started state, stopwatch and poll counter formation | 118 |
| Initial catch stores, raw error precedence, captured helper exit and return to finally | 88 |
| Fixed poll guard/exit work for one final evaluation | 52 |
| One reached poll body | 84 |

The environment unit has up to ten for loop control, 12 for the
dictionary getter, 12 for key and value indexed reads, 12 for
typed indexer store plus its formals and result, and six for
remaining operands and events. The two array references and
the dictionary remain distinct source owners. Initialization and
native Process.Start internals have their existing primitive
source boundary and separate resource obligations.

Let `P` be the eventual admitted parent poll cap shared by all three
helper slots. A continuing body requires a successful spend and increments
the poll count before sleeping. An expired clock or a refused
spend replaces a possible continuation. After P increments, the
next check cannot reach another spend. Count it as a final guard
in the fixed row. Within a body allow eight for HasExited and the
while result, 38 for both nested limit lookups/comparisons and
short circuits, 12 for the host abort flag/throw path, 18 for
the spend site's omitted unit count and conversion, call and increment,
and eight for the typed Sleep call
and loop event. The fixed rows including all eight environment
stores give 1,396 before the argument and poll terms.

## Mandatory parent cleanup and row formation

| Local region | Actions |
| --- | ---: |
| Incomplete parent watch check, kill, one wait, observed exit or return to diagnostic handler | 220 |
| Terminal-read dispatch and its catch/secondary path | 115 |
| Returned prefix bindings, cleanup and terminal-diagnostic calls, observed/committed tests and tail error path | 500 |
| Flags, signed raw exit, positions, known-count arithmetic and status/failure projection | 520 |
| Missing-terminal cleanup and explicit null projections | 136 |
| Quarantine, alternative Dispose and handler, cleanup extent guards and diagnostic maps | 440 |
| Undrained state, 24-field ordered map, cleanup projection, receipt append and first raw exit call | 230 |
| After-helper binding and failure route, diagnostic retention, map and cleanup projection replacement | 280 |
| Outer cleanup catch/fallback map and second raw exit call, final error precedence and return | 260 |
| Three rooted-marker membership sites at up to 15 reached entries × 14 | 630 |
| Fixed failure-reason membership up to 12 visits at 12 | 144 |
| Total local cleanup | 3,475 |

The pre-exit reap group owns the selected instance getters, Kill and
WaitForExit argument/binding/result. There is one possible kill and
one 2,000-ms wait argument, with no iteration. The terminal call owns
four arguments and its returned prefix assignment. Each catch and
the outer handler include the selected caught input, ordinary null and
precedence tests, and secondary call argument expression/dispatch.
No duplicate read or dispatch is attached to a direct result store.

The flag and result rows include U32 flag masks, all conditional
expression results, convert-to-Int64 raw exit subtraction and explicit
Int32 cast, three pipe position reads or unavailable branches, and
written/observed/unretained/omitted additions and assignment. The
ordered row owns its literal keys, 24 selected values and destinations,
and one first cleanup array result. The after-identity branch can
replace the cleanup projection, and the outer catch can form one
fallback quarantine map. All these branches are separately reserved.
The returned prefix, missing-terminal and Dispose alternatives share
some call sites; the upper counts each group once even when exclusive.

The source has at most two binding body calls, one target hash body,
one environment body, one request hash/body/argument each, at most
one terminal read, and the terminal projection calls in the separate
worksheet. It can reach up to eight `Set-TaskSecondary` and two
`Set-TaskRawExit` sites in the summed schedule. The parent spend and
per-argument string-validation bodies also remain separate. The outer
caller and source fence have their own entries.

Across the three slots with 28, 6 and 6 argument visits, the local
upper is **15,413 + 252P**. It bounds reached source
and local control across refusals. The parent caps, native launch and
watch timing, full source quotient and resource fit still need their
own acceptance. Independent review accepted the local call, typed
work and cleanup schedule.
