# Research typed local: annotation and identity null remain distinct

For the reached local declaration `[byte[]]$taskBuffer=$null`, count one explicit
type-reference R, one null-value R and the initialization W. This is a source
semantic annotation/application, not a claim that reflection resolves the type
again at runtime. Do not reread every formal parameter type on every function
entry or expand binding/type-metadata/cache internals.

No additional nonidentity conversion O is required for this null-to-reference
array initialization. Selected LanguagePrimitives.ConvertNullToRef returns the
original null value, and FigureConversionFromNull selects that route for a
non-value target type. Later bare assignments from the already exact byte[]
buffers do not repeat the declaration's explicit annotation or require a
nonidentity conversion. No byte referent cells are copied by these references.

This left-hand-side constraint is distinct from a reached explicit right-hand
cast operator. The selected parser's AttributedExpression.SetValue applies the
attributes through CallSetVariable; VisitAttributedExpression and
VisitConvertExpression have separate code-generation paths. The SDK body stays
opaque under the research primitive convention.

Primary source pins, all under the existing selected PowerShell inventory:

- parser/ast.cs:8AE1D4A25B930109C23F86D2FD2464ACBAB767FAE5471B3E01EB241D7D3A3F7C,
  lines7888-7898.
- parser/Compiler.cs:C5DED9509B65F2CD81B1D5B11F8915A220B32BE038599FC7008BA235563B95DC,
  lines6099 and6647-6650.
- LanguagePrimitives.cs:284B0FAD37EF182C51566E8135B20E4C27464B1EE9352A1786E7DC76C3089711,
  lines4247-4258 and5838-5844.

Original conventions and frozen proposals stay unchanged. This narrow boundary
classification accepts no replacement row/whole price, Data, resource fit,
native qualification or target execution. R2 still needs fresh whole review.
