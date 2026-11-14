# Clock R22 codec meter calls

These are per-helper upper bounds on calls and requested amounts in
profile validation, command parsing, launch-data formatting and terminal
record encoding. They include the debit that refuses.
[Request and hash families](2026-11-14-clock-r20-identity-read-census.md)
are counted in their own worksheet.

| Family | Calls | Sum of requested amounts |
| --- | ---: | ---: |
| `Entry` | 1 | 1 |
| `Profile` | 1 | 1 |
| `RawCommand` | 1 | 1 |
| `CommandCell` | 1 | 2,576 |
| `TargetQuote` | 1 | 1 |
| `QuoteCell` | 28 | 14,336 |
| `Environment` | 1 | 1 |
| `EnvironmentCell` | 8 | 4,155 |
| `Encode` | 1 | 1 |
| `EncodeCell` | 1 | 308 |
| `TerminalValidation` | 2 | 2 |

The supervisor invokes profile validation and raw command parsing at most
once each. Command parsing initially debits the full 2,576-unit capacity.
Target command and environment construction each have one body admission.
The command loop can debit 28 arguments with lengths up to 512 units.
The environment processes eight distinct names totaling 43 units. Each
debit includes its value up to 512 units and two delimiter units, totaling
at most 43 + 8 × (512 + 2) = 4,155. The final NUL is separate from the
per-entry debit expression.

Finalization may call terminal validation once. A current request can
then make one publication attempt, which enters the encoder once.
The encoder can debit 308 bytes and reach a second validation.
The [source and arithmetic check](artifacts/2026-11-14-clock-codec-meter/check.py)
asserts these totals against the pinned source and the literal site inventory.
