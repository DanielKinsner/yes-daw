# ClockOracle budget arithmetic correction

2026-11-05. The earlier ClockOracle R4 report's partial lower-bound arithmetic
was wrong. Its quoted 17,821,237,248 subtotal does **not** establish NOFIT against
2^34. The source, sealed reports and historical review records remain unchanged;
this correction supersedes their numerical conclusion wherever repeated.

The local sealed report is
`build-ci/20261105-clock-oracle-r4-cpp-census/census.md`, SHA256
`B415336A0A7576EAAB422BF7D12F19EDB419B2D9A20950176937E39E2830BCC4`.
Its JSON companion is SHA256
`10935399D84D18108E2C9C3BFA871DC5257B57ABC06258623DB6491E08303283`.

| Listed expression groups | Reported | Actual sum |
| --- | ---: | ---: |
| RationalCut: 14+12+10+4+5+28+7+2+42 | 224 | 124 |
| frameForTick: 2+8+2+11+4+5+7+7+9+9+26+2+3 | 105 | 95 |
| frameForMusicalTime groups | 122 | 122 |
| Production body: 16+122+corrected Tick subtotal | 243 | 233 |
| CachedPort fractional groups | 69 | 69 |

Using the report's unchanged quoted multiplicities gives the following arithmetic:

```
2 * 16,929,297 * 124       =  4,198,465,656
2 * 16,405,308 * 233       =  7,644,873,528
2 * 16,405,308 * 69        =  2,263,932,504
sum                       = 14,107,271,688
2^34                      = 17,179,869,184
difference below 2^34     =  3,072,597,496
```

This corrects the addition of the listed groups. It is not a new source-lower
certificate, a complete upper bound, or evidence that the full protocol fits.
All omitted C++ work and the entire runner still need complete independent
censuses. The old R4 source retains its original cap and false guards; its actual
complete fit is **unknown**, not demonstrated NOFIT and not demonstrated fit.

The separately accepted prospective research contract344408DA remains a sealed
historical document. Its repeated NOFIT premise is corrected here. It selected no
new work allowance, and supplies no execution authority. Continuing source
preparation must stand on its full-census/derive-U-last requirements independently
of the erroneous lower bound, with fresh independent review before any binding
or execution. No convenient larger allowance follows from this correction.

R6's whole functional-source acceptance is separate from resource admission. Its
runner census independently found an unbounded poll multiplicity and repeated
explicit hash buffers without a reclamation/reuse proof. Those obligations remain
open. Production caps, numerical tolerances, coverage, resource ceilings, old source,
first-GREEN proofs, goldens and annotations are unchanged.
