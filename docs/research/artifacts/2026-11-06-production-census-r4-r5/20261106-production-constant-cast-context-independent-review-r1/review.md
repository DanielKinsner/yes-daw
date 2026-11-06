# Required constant evaluation and ordinary casts

Verdict: **ACCEPT the existing source-action boundary; retain ordinary nonidentity conversions.** This supplements, without changing, the separate constant-expression boundary review. It certifies no complete vector or source price.

Required constant-evaluation contexts, including constexpr initialization, static_assert, template arguments and constant array extents, do not execute runtime source actions. The operand of sizeof is unevaluated. Ordinary function-body arithmetic, conditional selection and nonidentity conversion remain reached algorithmic actions under ADR0085:176–185 and conventions404E, regardless of possible compiler folding. A constexpr operand or constexpr function declaration does not itself make every later use or invocation a required constant-evaluation context.

Concrete selected R5 source:

- window-query.h:552: `static_cast<std::size_t>(NativeFact::Count)` converts a scoped enum to size_t in an ordinary comparison. NativeFact has uint8_t underlying type at native-profile.h:77, but is a distinct scoped enum type. Retain X1. No scalar read is invented for the enumerator.
- window-query.h:885: `static_cast<double>(engine::kTicksPerQuarter)` converts Tick (int64_t, src/engine/Time.h:43 and45) to double in ordinary endpoint arithmetic. Retain X1. The constant's definition does not move this body expression into a constexpr initializer. The surrounding written multiplication/division operations remain separate source actions.
- native-observer.cpp:102: `static_cast<std::uint64_t>(sizeof(info))` converts a size_t value. Under the independently object-qualified selected R5 d0 identity between size_t and uint64_t, that cast is an identity and contributes X0. Generic accounting retains the separate d parameter; this note supplies no new ABI observation.
- native-observer.cpp:82: `static_cast<DWORD>(sizeof(info))` is a nonidentity conversion from the selected x64 size_t to DWORD. Retain X1. An unevaluated sizeof operand does not erase a surrounding conversion.
- formation-r8.h:462–464 constexpr count initializers and window-query.h:211/native-profile.h:129 constant array extents are compile-time contexts. They are distinct from the ordinary runtime helper casts at formation-r8.h:723–725. Declaring those helpers constexpr does not make their ordinary calls free.

This is the same criterion used for reached ordinary Reader ternaries and the byteCap division/subtraction in the separately pinned constant-expression note. Plain compile-time qualification and literal production stay free of invented R/X; actual reached operators are not discounted into an optimized machine trace.

Primary semantics: [constant expressions](https://eel.is/c++draft/expr.const), [sizeof](https://eel.is/c++draft/expr.sizeof), [conditional operator](https://eel.is/c++draft/expr.cond). No compilation, disassembly, parser, AST, helper, target program, stage or GUI was used. Exact ABI association, complete source vectors, prices, resources and all execution gates remain separate.
