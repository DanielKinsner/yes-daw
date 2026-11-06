# R4 production census independent numeric review

**REVISE.** The complete sealed proposal has two source-counting defects in returned getters. The packet's own vector/helper/schedule arithmetic and its Markdown/JSON correspondence reproduce, but they propagate these omissions. The subject packet is preserved.

This review uses production conventions404E214B and clarification709B8A43, not the Clock research tariff. It reviews the420-row packet and its38 schedules/12 quote terms as a whole; no positive acceptance of the other literal source prices is implied by the mechanically consistent arithmetic. The blocking concrete cases below suffice to reject the whole proposal. A corrected whole packet still requires fresh independent review.

1. GetterTwo at census.json:1180 / census.md:601 has `(2,2,0,0,0,1,1,0,0,0)`,price6. Six concrete calls to our limits/policy/mappedWindow getters return a two-cell member lvalue by value. They need getter H/Z plus the selected copy H/Z: `(2,2,0,0,0,2,2,0,0,0)`,price8. The existing2R/2W count the transfer once. No additional zero initialization, DMI or result copy is invented.
2. GetterReference is misapplied to `clock->rows()` at legacy-adapter.h:273 and window-query.h:949. MusicalAudioWindow.h:162 returns a by-value std::span constructed from the vector. The selected MSVC span range constructor at include/span:341-350 takes logical data and size. The complete logical getter case is2R/2W plus getter and constructor H/Z,price8, replacing price2. Neither caller body already contains the span result destinations. QueryPredictionEndpoints uses its separate GetterTwo for limits() at950; rate at951 is a direct metadata field access.

The return statement initializes the result from its operand; the member lvalue does not satisfy the local-object elision condition. These distinctions follow [stmt.return](https://eel.is/c++draft/stmt.return) and [class.copy.elision](https://eel.is/c++draft/class.copy.elision), together with the adopted production boundary. They do not change the tariff or expand SDK forwarding internals.

|Site|Sealed price|Necessary price if other components are retained|
|---|---:|---:|
|162 LegacyClockCumulative|179|185|
|168 LegacyRequestBytes|144|146|
|175 LegacyPlanPreflight|154|156|
|176 LegacyPlanOwner|118|120|
|179 LegacyPlanBytes|168|172|
|343 QueryPredictionEndpoints|168+d|176+d|

The resulting necessary schedule corrections are LegacyFixed24449+5d (+16), PredictFixed146073+d (+8), ResolveFixed154189+d (+8), QueryNewFixed161638+d (+8), and root51747+9B+8d (+16). Other38-schedule coefficients have no change from these two findings. Query quote term1 increases8; the other11 terms retain their sealed coefficients. LegacyCheck2724, LegacyCleanup1997, native fixed/region/page/byte coefficients and all five role reserves have no change from these findings. These are consequences of retaining all other components, not accepted replacement prices.

The illustrative same-Origin totals consequently increase24. Its conditional R1 Q becomes11804882; the illustrative total excluding N becomes11905951 and C1/N1 becomes11905975. Corresponding illustrative C1/C2 maximum N values become10688729/6413237. No actual native data, complete source-price acceptance or fit proof follows. Unused reserve cannot substitute for fixing the underpriced Sites. The unchanged2^28 cap and failure/refusal gates remain in force.

2,826 reproducible file/data checks cover the three packet seals,23 pinned subject/reference/evidence/output records,50 helper sums,420 full factorized vectors and scalar coefficients,38 schedule sums, every420-row Markdown price,50 helper Markdown rows,38 schedule Markdown rows,190 exact old vectors and78 compositions, the full five-role root equation, twelve quote forms and conditional examples. The independent checker never runs any author's helper. Detailed results and exact corrected vectors are in evidence/packet-checks.json; concrete return types and caller destination ownership are in evidence/getter-cases.json.

The old190 protected callee vectors remain byte/data-identical and are not rewritten. The finite schedule design still explicitly holds all five role lifetimes, Result-drop then Retained/Reader sequential use, two physical result cells, six stage/output owners, and repeated restore prechecks. Explicit scalar maxima (search alternatives, native byte continuation/mismatch, reconciliation, acquire/abort and prefix failure+reject) are source-supported reservation alternatives; they are not actual visit counts. Aggregate braces retain125 writes with no phantom initialization constructors; default-init result members retain the separately sponsored4 DMIs/3 constructor edges. None of these observations waives the two new getter defects or accepts all remaining literal source-body counts.

Required next step: author a separate corrected census packet with concrete getter cases, recompute every dependent equation/example and update MD/JSON/pins, then obtain fresh whole numeric review. Preserve this R1 packet. R4's archived C4324 compile failure remains historical; the separately accepted R5 declaration-order correspondence and current37-type constant layout record supply no automatic R4/R5 size/offset equality, price binding, resource fit, all-input/native/forward or whole API/GO credit. All immutable Data/Catalogue definitions and execution gates remain held.

Only read-only source/data/hash work and our own Python integer/JSON checker were performed. No target PowerShell parse/AST/import/helper, compiler/linker, program/test/stage/native API, GUI/audio or Git/CI was executed. No subject source, census, old catalogue or evidence was changed.
