# Ordinary constant operands retain reached source operators

Verdict: **ACCEPT this boundary interpretation; REVISE the proposed constant-folding discounts.** This is an independent narrow interpretation of the existing production logical-action boundary, not a new tariff or a whole numeric acceptance. No subject source was changed or executed.

ADR0085:176–185 counts each reached check/comparison and checked arithmetic/address step as an algorithmic source action. Proposal95DF:28–29/45–47 counts every reached action and preserves our conditions. Conventions404E:26–27 keeps our conditions expanded. The accepted boundary does not replace the source model with an optimized instruction trace.

In selected R5 formation-r8.h:1512, Reader is a bool non-type template parameter. Its value requires no runtime scalar read. However, the ordinary conditional expressions written in function bodies still evaluate their first operand and select a result under C++ [expr.cond]/1. Fixing a template argument's value under [temp.arg.nontype]/2 does not make all uses of that value required constant-evaluation contexts. These bodies do not use if constexpr or a constexpr initializer for the disputed expressions.

| Body | Actual state-dependent C | Ordinary Reader ?: C | Total C |
|---|---:|---:|---:|
| Receive,1522–1533 | 2: signal and context | 4: Role1528,Site1529,context1530,context1531 | 6 |
| ContextReturn,1537–1549 | 1: refs1542 | 3: Role1540,context1543,Role1545 | 4 |
| ContextCancel,1550–1557 | 1: cancelChecked1554 | 1: Role1553 | 2 |
| ContextInspect,1558–1571 | 5: inspected1562,status1566,two IDs1568–1569,final mask1570 | 1: Role1561 | 6 |

The metadataLive_ and parentLive_ values at1567 are copied bool values in eager bitwise arithmetic. Their reads and written casts remain separately owned; they are not extra conditional checks. The signal load and helper/caller/role ownership stay independently counted. This table certifies only C for these four bodies, not complete vectors or schedules.

The same boundary applies to CaptureCheck1006 `byteCap / sizeof(engine::TempoChange)` and CaptureProducts1020 `byteCap - sizeof(MusicalPreparationRequestState)`. Each enclosing ordinary `/` or `-` retains one reached arithmetic X. The sizeof operand is unevaluated and contributes no referent read; that does not erase the surrounding operator. Thus the proposed X14→12 and X4→3 discounts cannot be supported by constant folding. This conclusion does not independently certify every other component of X14 or X4.

By contrast, the static_assert expressions at620–624 and703 are compile-time requirements; the constexpr declarations at22–23 initialize constants. There is no runtime charge for evaluating those declarations/assertions. Plain namespace/class qualification and constant value production retain the previously reviewed naming boundary; no extra R or X is invented for `Reader`, `byteCap`, or `sizeof` itself. An actual written runtime comparison, arithmetic operation, designator or conversion remains separately charged.

Primary C++ sources: [conditional operator](https://eel.is/c++draft/expr.cond), [constant template arguments](https://eel.is/c++draft/temp.arg.nontype). These establish source semantics, not an observation of compiler output. No compiler, parser, AST, target helper, target test, program, native API, stage or GUI was invoked. All prices, complete source envelopes, binding, resources and execution authority remain pending their separate reviews.
