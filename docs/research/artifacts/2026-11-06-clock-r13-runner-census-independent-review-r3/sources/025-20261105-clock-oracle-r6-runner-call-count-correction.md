# R6 runner census table wording correction

The sealed R6 runner census remains unchanged and HOLD. Its helper table's
Assert-TaskConsolePE row overstates ordinary calls: the actual source has THREE
ordinary invocations (compiler, built child, baseline GUI-copy image), plus ONE
expected GUI-subsystem rejection, FOUR total. Get-TaskPEHeader has FOUR normal
calls; its fourth reads the mutated GUI image before that rejection.

The Read-TaskChildResult row accidentally repeats console-check wording. Its
actual callers are TWO result reads, one producer and one verifier; the same row's
3+11 accepted controls,84 operand normalizations and24 summary copies describe
those two calls. Neither correction closes the independent poll or explicit-buffer
HOLD or grants any numeric price, fit or execution authority.

Root read the actual R6 main caller graph and recorded these corrections after
the original packet was sealed. Future source/census must derive actual calls
rather than inherit the erroneous table wording.
