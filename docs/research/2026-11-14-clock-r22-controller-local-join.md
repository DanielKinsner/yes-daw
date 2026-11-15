# Clock R22 controller local work join

The [headless check](artifacts/2026-11-14-clock-r22-controller-delta/local-work.py)
verifies the pinned runner and manifest, checks written calls into the
19 changed or added bodies, and composes their reviewed schedules. Run
`python -I -S -B docs/research/artifacts/2026-11-14-clock-r22-controller-delta/local-work.py`
with the exact R22 source staged. `P` is the shared, positive parent
poll limit for each child slot. The manifest leaves it unset. The
native target policy's 7,200 polls and the signed Int32 representation
test supply no selected parent limit.

| Body scope | Fixed actions | Per unit of `P` |
| --- | ---: | ---: |
| [Transport](2026-11-14-clock-r22-controller-transport-work.md), six bodies | 2,658,596 | 0 |
| [Binding and meter](2026-11-14-clock-r22-controller-binding-work.md), three bodies | 13,472 | 600 |
| [Child invocation](2026-11-14-clock-r22-controller-child-work.md), one body | 15,413 | 252 |
| [Terminal semantics](2026-11-14-clock-r22-controller-terminal-work.md), five bodies | 19,083 | 0 |
| [Catalogue, fence and startup](2026-11-14-clock-r22-controller-fence-work.md), four bodies | 796,783 | 0 |
| All 19 bodies | **3,503,347** | **852** |
| [Root caller and registration](artifacts/2026-11-14-clock-r22-controller-delta/README.md#script-root-caller-work) | 188,619 | 0 |
| [Composed file hash](artifacts/2026-11-14-clock-r22-controller-delta/README.md#shared-file-hash-body) | 8,880,355 | 0 |
| Partial controller upper | **12,572,321** | **852** |

The root pays each invocation expression. The unchanged
`Invoke-TaskFencedChild` owns its before/after fence call expressions
and the call into `Invoke-TaskChild`. Its body remains in the inherited
schedule. Two root fences and two per child give eight entered fence
bodies. Each fence sponsors one binding body and each child can sponsor
two more, giving 14. The fence dispatch is already inside the fence
local upper. The transport row includes all 100 Set and 102 Get bodies.
The terminal reader and assertion own their nested invocation expressions.
`Add-TaskChildCleanup` has one registration and zero entered bodies.

## Edges into the inherited bodies

The following are maximum direct body visits sponsored by the 19 local
bodies. The written argument work and dispatch are already in the
local rows. Other inherited callers, descendants and root calls keep
their schedules.

| Inherited callee | Maximum visits from the local bodies |
| --- | ---: |
| `Add-TaskPathCatalogueEntry` | 1,288 `true` and 81 `false` |
| `Resolve-TaskInput` | 10,608 |
| `Assert-NoLink` | 324, plus separate inherited descendant edges |
| `Get-TaskEntries`, `Assert-TaskFileSet` | 304 each, both at the fence |
| `Assert-TaskFreshHashIdentity` | 10,350 |
| `Test-TaskHashValue` | 8 direct manifest tests |
| `Get-TaskHash` | 10,361 of the 10,513 in the composed hash row |
| `Get-TaskBootstrapHash`, `Assert-TaskCodecBinding` | 7 bootstrap hashes at 379 chunks, one codec binding |
| `Assert-TaskIntegral`, `Get-TaskPEHeader`, `Assert-TaskConsolePE` | 28, 14, 14 |
| `Assert-TaskExactKeys` | 3 eight-key calls |
| `Assert-TaskString` | 19 general at up to 512 units, 166 ASCII at up to 512, 14 ASCII at up to 64, and 3 ASCII at 22 |
| `Get-TaskStageBytes` | 6 |
| `Close-TaskOwner`, `Complete-TaskOwnerClose` | 6 each for the transport streams |
| `Set-TaskSecondary`, `Set-TaskRawExit` | 18 and 6 |
| `Assert-TaskBoundSourceInteger` | `121 + 3P` |
| `Spend-Task` | `12 + 3P` |

Each binding sponsors eight signed-integer checks. The other
`9 + 3P` checks follow the four supervisor price lookups. There are
`9 + 3P` corresponding `Spend-Task` bodies plus three direct helper
reservation spends. The composed hash row already includes its own
scoped Spend, Close and Complete bodies. `Assert-NoLink` and
`Get-TaskCanonicalPath` stay outside the hash price. The six transport
Close and Complete visits apply to the request and terminal files.
The seven bootstrap hashes have their own retirement scopes. The
former startup self-process Close and Complete edges were removed.

## Counter amounts and reservation

The four `controller_prices` entries multiply source counter amounts
at separate authored sites. Each entry is still unset in the staged
manifest. The amounts alongside their prices are:

| Price | Whole-run amount before multiplying by price | Debit position |
| --- | ---: | --- |
| `invoke` | 3 invocations | After child argument and binding checks, before request construction |
| `request` | 78,548 bytes | After string preflight and extent gate, before encoding and writing |
| `terminal` | 924 bytes | After file capture and retirement, before magic, decode and validation |
| `parent_poll` | `3P` poll bodies | After exit and watchdog guards, before increment and sleep |

Request limits are 41,260, 18,644 and 18,644. The three 308-byte
terminals give the rest of the transport allowance. A refusal can
execute work before its debit site or enter a mandatory cleanup after
it. The static source envelope includes that work. Counter amounts
measure authorized debits. Authored action prices come from the
worksheet bodies and need their own fit check.

The three full native helper reservations enter the counter at the
explicit `Spend-Task` in `Invoke-TaskChild`, before request creation.
The request repeats each slot's total/tail data. Observed terminal
prefix usage supplies no refund. One admissible `runner_upper`
must cover both the final authored controller upper plus the three
native reservations and the maximum reached controller counter
debits plus those same reservations. When composing
`runner_upper + producer_upper + verifier_upper`, the helper
reservations are already inside the first operand.

The partial upper still requires the other inherited body schedules,
the selected controller and helper prices, and a justified parent
poll and watchdog profile. It gives no execution or allocation bound.
