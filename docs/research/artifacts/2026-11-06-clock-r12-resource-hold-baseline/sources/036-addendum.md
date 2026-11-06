# R12 HOLD addendum — host-wide cleanup census

The R12 baseline folder `20261106-clock-r12-payload-evaluation-r1` was sealed immediately before this source finding. Its files and three seals are unchanged. This addendum makes one subterm of its explicitly unresolved V family precise; it changes no selected numeric partial, source, cap, domain or execution authority.

At runner line233, `Process.Kill(true)` can enter selected `Process.Win32.cs` `KillTree`. `GetProcessHandlePairs` at lines428–454 enumerates **GetProcesses() for the whole host** before applying its parent predicate. Each visited descendant can repeat that census recursively. The top-level child count, log byte cap and259 outer-read bound do not bound host process/thread records or descendant census passes.

Retain `V_host_process_census = sum_j Census(H_j, T_j, bufferCapacity_j, handleAttempts_j) + descendantLists/handles/predicate/call-stack owners + bounded source failure projections`, where `j` ranges over actual KillTree visits, `H_j` is host process count and `T_j` its required thread/info extent. These are nonzero external/native/source profile terms; no arbitrary per-Kill constant replaces them. Ordinary C# wrapper/array/record/list owners remain controlled. General SDK/native internals and exception/task-fault heap keep the baseline's explicit unqualified boundary.

`WaitForExit(2000)` runs after `Kill(true)` returns and does not itself bound the recursive census operation. The SDK attempts to kill a parent before enumerating descendants, but caught kill failures continue into enumeration; the current accepted child/log caps supply no complete descendant/census schedule bound. This remains HOLD pending source/resource qualification or a separately reviewed supported correction. No correction is selected here.

This is a public source-file read and hash check only. No process was enumerated, inspected, killed or launched; no host snapshot, target execution, GUI, Git or CI was used. It proves neither32MiB fit nor a coexisting violation/NOFIT.
