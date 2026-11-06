The initial independent correspondence checker returned ordinary Python exit1
while serializing its own evidence, before producing mechanical-evidence.json:

`TypeError: Object of type bytes is not JSON serializable`

It stored the raw byte objects used in an unchanged-file comparison inside its
JSON check records. The corrected checker records the boolean equality instead.
No target code, PowerShell parser/helper/compiler/program/stage was invoked.
The original checker is preserved beside this note; no subject bytes changed.
