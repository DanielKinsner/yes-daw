# Clock R22 controller catalogue, startup and pin fence work

This proposal covers the four changed controller bodies outside child
invocation. The runner SHA-256 is
`294C4D91D0FF0EB0E80048B71DFC1E24C45103E88AD97F2F7715F9BD252F4798`
and the manifest SHA-256 is
`28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06`.
The [source correspondence](artifacts/2026-11-14-clock-r22-controller-delta/README.md)
and the [R13 caller convention](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/206-argument.md)
identify the source boundaries. Scalar terms count caller argument
evaluation, dispatch, actual conversions, declared primitive ports,
and real destinations. Script bodies called from these regions are
priced separately. The R13 review left whole prices open, so the
[R13 body rows](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/250-body-ledgers.json)
serve as source witnesses for the common expressions checked here.

In the tables, ordinary indexed reads use the receiver, index,
selection and result/argument ports. Compound map reads retain each
selector. An instance getter pays its receiver, dispatch, entry,
return and destination. A static comparer getter pays its type
selection and boundary, with the result consumed by the receiving
constructor formal. Collection and comparer internals retain the
managed primitive boundary. The script's arrays, loops, casts,
comparisons and callbacks retain their source work.

## Initialize-TaskPathCatalogue, lines 86–102

The manifest has 1,288 historical pins, four current own-source
names, one external manifest, 56 references, three compiler outputs,
four controls, six stage files, six supervisor transport files and
one supervisor binary. The 75 root-run identities cover the 74
success paths and one failure extra. The catalogue therefore admits
`1288 + 4 + 1 + 75 + 1 = 1369` identities. Its bound arithmetic
gives 2,819 alias slots and 88,985 parent-string slots. These are
logical ceilings for the source visits and allocations.

| Local region | Upper |
| --- | ---: |
| Entry and return | 2 |
| Derived maximum expression | 49 |
| Profile equality, range, and product tests | 36 |
| Ten-field catalogue map | 102 |
| Named own-source foreach, plus five top-level indexed loops and one nested indexed loop | `4 + 5×7 + 10 = 49` |
| Two standalone Add callers | 6 |
| Final rows Count and comparison | 13 |
| One selected explicit diagnostic | 4 |
| Historical pin callers | `1288 × 11 = 14,168` |
| Own-source, stage and transport name callers | `(4 + 6 + 6) × 15 = 240` |
| Reference, compiler-output and control descriptor callers | `(56 + 3 + 4) × 18 = 1,134` |
| Local upper | **15,803** |

The maximum group includes the inputs and own-name Count reads, two
profile cell reads, the Long cast, two written one literals, five
additions and the final assignment. Its 49 allows the typed operand
and intermediate-result actions of the sum. The 36 group covers
four tests, two Int32 maximum reads, the Long multiplication and
short circuits, including mixed numeric operands. The map group
includes its ten key/value slots, two explicit Int32 casts, two
input Count reads and typed alias arithmetic, list construction,
and all four Ordinal comparer getters and dictionary constructors.

The foreach frame uses four events for a named enumerable and seven
for a single selector. The supervisor transport enumerable has three
more for its second selector. Each reached historical row uses five
foreach actions, four for the path selection and two for the literal
Boolean and Add dispatch. Each joined name row adds the static Join
type, two source values, two parameter destinations and primitive
boundary in place of the path selection. The descriptor row reads
an indexed path or name in place of the simple name. The external
manifest and supervisor binary each supply a path and false to one
direct call. An entered Add can stop on the row capacity or a
duplicate dictionary key.

Compose `Add-TaskPathCatalogueEntry` separately at most 1,288
times with true and 81 times with false. Both port shapes are
byte-identical to R13. The six transport additions and one binary
addition are new source edges. First path use can still build the
bounded canonical string/parent chain in `Get-TaskCatalogueEntryPath`.
Track that work at its later resolver or canonicalizer callers.

## Assert-TaskPins, lines 1054–1067

The fence calls the new supervisor binding once before its inherited
enumeration, manifest, own-source and historical pin work. An early
binding refusal can end the fence there. The 38 frozen namespaces have
1,115 declared entries and 1,068 declared files. Namespace `i` passes
`entry_count[i] + 1` to `Get-TaskEntries`, giving an aggregate 1,153
row return ceiling. An extra candidate can reach its over-cap check
inside the enumerator callee. A refused enumeration skips the later
FileSet comparison. File or directory churn can affect the returned
mix. Use 1,153 for both authored callbacks' per-fence occurrence
ceiling, including the projection on a FileSet refusal prefix.

| Local region | Upper |
| --- | ---: |
| Entry/return, binding dispatch, three indexed foreach frames | `3 + 21 = 24` |
| Manifest hash and predicate callers, with the explicit Byte[] cast and Long cap conversion | `5 + 12 = 17` |
| Own-name Ordinal HashSet and inventory extent test | `10 + 17 = 27` |
| Input byte accumulator and final exact byte/count profile guards | `2 + 28 = 30` |
| One selected explicit diagnostic | 4 |
| Namespace loop and helper/command callers | `38 × 58 = 2,204` |
| Filter and projection bodies | `1153 × (12 + 29) = 47,273` |
| Own-name, Join, pin cells, hash and fresh check callers | `4 × 54 = 216` |
| Historical pin, resolve, hash and fresh check callers, with signed accumulator | `1288 × 37 = 47,656` |
| Local upper per entered fence | **97,451** |

The closed-namespace row budgets five actions for foreach control,
eight for the Resolve/NoLink calls, 15 for Entries including the
written Int32 cast of the pinned entry count, addition, Boolean
selection and omitted null destination argument, nine for the
filter command and array capture, 11 for the projection command and
capture, and ten for the FileSet call and its typed string-array
arguments. The projection command includes the authored block
binding and scalar-block-to-Process-array conversion. Each filter
callback uses dispatch/entry/return, automatic input destination,
the Item3 getter, predicate negation and result (12). Each
projection callback includes those initial four boundary actions,
Item1, GetRelativePath, two explicit Char type/conversion pairs,
Replace, and the fused destination (29).

The 54-action own row consists of foreach five, 20 for membership,
ordinal Add and refusal control, 12 for Join including the scalar
Path-to-array conversion, ten for the two pin cells and seven for
the nested hash and freshness calls. The 37-action historical row
includes foreach five, resolver and local destination six, the
two pin cells ten, both nested hash/identity callers seven and
nine for the typed Long accumulator, including the written cast.
The function terminates on its first reached refusal. Its caller
can still enter a separate finally fence.

| Separate callee or source visit within the fence | Per fence | Eight-fence upper |
| --- | ---: | ---: |
| Supervisor binding | 1 | 8 |
| Resolve-TaskInput | 1,326 | 10,608 |
| Direct Assert-NoLink at each namespace root | 38 | 304 |
| Get-TaskEntries | 38 | 304 |
| Enumeration candidate work, including the next excess candidate per enumeration | 1,191 | 9,528 |
| Directory-iterator visits within Get-TaskEntries | 727 | 5,816 |
| Assert-TaskFileSet | 38 | 304 |
| FileSet expected and observed names, respectively | 1,068 and 1,153 | 8,544 and 9,224 |
| Get-TaskHash, including current manifest | 1,293 | 10,344 |
| Chunk visits in those Get-TaskHash calls | 8,021 | 64,168 |
| Assert-TaskFreshHashIdentity | 1,292 | 10,336 |
| Direct manifest Test-TaskHashValue | 1 | 8 |

The directory visit rule allows one root per namespace and all
admitted entries as queued directories in each recursive namespace.
The 1,191 candidates allow one subsequent excess candidate in each
invocation. Each directory visit can call NoLink and reach its own
iterator retirement. Each hash call can also call NoLink. The
aggregate for those NoLink edges plus the direct roots is 2,058
per fence, excluding binding's separate subtree. The 8,021 chunks
are 7,999 historical, six own and 16 under the current manifest's
one-MiB cap, using per-file rounded-up chunks. Hash prefix failure
can end before a chunk or during it. A false manifest hash can
end the fence after its one direct test. The fresh identity
callee's own hash-test and rejection work stay in that callee
schedule. One FileSet refusal per fence can reach up to two
bounded diagnostic name joins.

Two main sites and the pre- and finally fences for each of three
child positions give at most eight entered fences. The frozen
manifest also carries a nine-fence prepaid profile field. There
is no ninth direct caller in this graph. The eight binding
occurrences above are part of the 14 already included in the
[binding worksheet](2026-11-14-clock-r22-controller-binding-work.md).

## Initialize-TaskController, lines 556–596

The fixed runtime directory literal moved to the local R22 profile.
The seven identity rows, caps and bootstrap hash caller shapes
are unchanged. R22 queries the static Environment ProcessPath
property, rejects null, validates a general bounded string, removes
an optional extended-path prefix and compares to the joined pinned
pwsh.exe path using OrdinalIgnoreCase.

| Local region | Upper |
| --- | ---: |
| Entry/return and one selected explicit diagnostic | 6 |
| Seven forbidden-startup-variable sites | `18 + 7×15 = 123` |
| Runtime literal and seven three-field pin arrays | `2 + 58 = 60` |
| Seven hash callers, path joins and exact-result checks | `4 + 7×30 = 214` |
| ProcessPath query and gates, detailed below | 55 |
| Command-line query, explicit 14-slot array and exact extent check | `5 + 37 + 14 = 56` |
| Property-bounded argument loop and local validation/equality callers | `13 + 14×14 + 14×31 = 643` |
| Apphost normalization and both executable/DLL equality paths | `10 + 42 = 52` |
| Autoload assignment, two module calls and two command lookups | `2 + 32 + 26 = 60` |
| Implementing types, two assembly location checks and binding caller | `18 + 84 + 1 = 103` |
| Local upper | **1,372** |

The ProcessPath group is five for the static getter and local,
four for null comparison and branch, four for the string validator
operands, default and dispatch, 13 for StartsWith with the
Ordinal static enum field, eight for optional Substring and
direct assignment, and 21 for the joined path's ordinal-ignore-case
Equals and its guard. The actual string validation body is
separate. The seven boot rows use their exact Long cap literals
and two indexed fields for each hash call/result check. The string
argument loop pays the bounded property loop's 13 events plus 14
per visit, two operand selectors for an equality, receiver and
parameter initialization, static enum selection, command result
and short-circuit branch within the 31 caller-action envelope.
The equality is reachable only at indices 1 through 13. The
14-slot literal includes its one Join result and the argument
guard checks Count before visiting any values. Module and codec
setup includes both scalar-array argument conversions for Name,
the Stop and Cmdlet enum conversions, selected destination writes,
the implementing type getters and the assembly/location getter
chain. Real loaded-command/runtime association retains its own
profile evidence.

Compose seven `Get-TaskBootstrapHash` bodies at their pinned
sizes (379 chunks in total), one general `Assert-TaskString`
body (at most 512 units for an in-domain ProcessPath value), 14
ASCII validator bodies (at most 7,168 total units), and one
`Assert-TaskCodecBinding` body. A length rejection can reach a
validator's fixed prefix with no unit loop. The runtime path query
and argument query are primitive boundaries. Their cache and
allocation behavior need the selected fresh-host profile.

The old `GetCurrentProcess` and MainModule/FileName getter chain
and self-owner catch/finally are gone. Remove those two direct
retirement edges, one Close and one Complete, when building the
R22 schedule. The seven bootstrap hashes still own their normal
stream/hasher retirement, at most 14 Close and seven Complete
calls. Other retirement sites also keep their own source scope.

## Add-TaskChildCleanup, lines 496–503

The retained script body replaces the old composite maximum with
the single Int32 literal 15. Its local scope has six for entry,
four formals and return, two for the maximum, nine for Count and
its guard, four for the ASCII validator caller, seven for the
typed List Add boundary, eight for the raw/primary tests, three
for the larger of the primary assignment and secondary caller,
and four for the selected diagnostic. That is **43 local
actions per entered call**. A caller would still sponsor one
separate ASCII validator body at up to 64 units and one secondary
body on the nonnull/already-retained route.

R13 called this helper during managed child retirement. The R22
controller has its declaration and no call edge. R22 invokes
`Add-TaskSupervisorCleanup` for the native prefix and uses direct
List Add, error retention, and quarantine paths in `Invoke-TaskChild`.
Those reached paths belong to the
[terminal](2026-11-14-clock-r22-controller-terminal-work.md) and
[child caller](2026-11-14-clock-r22-controller-child-work.md)
worksheets. The zero occurrence applies to this declaration alone.

The reachable local-body subtotal is
`15,803 + 8 × 97,451 + 1,372 =` **796,783 source actions**.
The callee schedules above remain separate. Independent review
matched the exact sources, typed slots, call graph, current namespace
counts and all four local operands.
