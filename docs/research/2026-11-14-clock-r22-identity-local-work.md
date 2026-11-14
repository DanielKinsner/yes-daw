# Clock R22 request and identity local source work

The independently reviewed rows below compose once per helper. Every
call to initialize, feed, pad or compress SHA is accounted for in the
SHA worksheets. The file and run-identity caller rows account for
their own branches and local work.

| Component | Source actions per helper |
| --- | ---: |
| [Request reading and both file hash bodies](2026-11-14-clock-r22-identity-io-scalar-census.md) | 3,324,531,808 |
| [Run identity input construction](2026-11-14-clock-r22-run-identity-scalar-census.md) | 28,896 |
| [SHA initialization, update and padding](2026-11-14-clock-r22-sha-stream-census.md) | 4,155,708,564 |
| [SHA compression and rotation](2026-11-14-clock-r22-sha-compress-census.md) | 11,735,179,168 |
| Local subtotal | 19,215,448,436 |

The room reserved for invocation and result handling includes early
refusals. SHA call boundaries assign argument evaluation and binding
once to the callee worksheets. Local file and request diagnostic
allowances include their direct error routes. Meter-induced diagnostics
and handoff work remain with the meter.

Path validation, recipe parsing and string comparisons, meter and its
cut handling, owner close, source sponsorship by phase, native time
and resource lifetime remain separate composition terms. The local
subtotal does not supply the whole helper price.
