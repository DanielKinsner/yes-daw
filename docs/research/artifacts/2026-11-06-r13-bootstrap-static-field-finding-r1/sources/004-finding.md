# R13 bootstrap static-field correspondence finding

SOURCE-LEVEL DEFECT; no runtime attempt or exact thrown exception is claimed. Preserve R13 and every historical source/review unchanged. The new R14 candidate must correct the expression and receive fresh whole functional/census/resource review. R13's1491 previously passing source checks do not establish this missing bootstrap correspondence. Conditional scalar counting can continue against its exact frozen source, without executable or whole functional acceptance.

R13 run-clock.ps1 line49 initializes `$taskObjectGetType` with `[object].GetMethod('GetType',[Type[]]::EmptyTypes)`. The represented receiver of that static field access is System.Type[], not System.Type.

The selected PowerShell source is commit ea554a8f9c6085f54fb2828f7ea286c65c35599f:

- Binders.cs5653–5660 routes static lookup directly to PSObject.GetStaticCLRMember(target.Value, Name).
- MshObject.cs1005–1014 unwraps the object, uses `obj as Type ?? obj.GetType()`, then calls DotNetStaticAdapter.BaseGetMember on that represented type. It does not substitute an array element type.
- CoreAdapter.cs3636–3646 chooses GetStaticPropertyReflectionTable((Type)obj);3462–3477 fills that table with the same represented type; its property/field population queries that type's public reflection fields. This path supplies no array-element static-member fallback.
- The public field [Type.EmptyTypes](https://learn.microsoft.com/en-us/dotnet/api/system.type.emptytypes?view=net-10.0) is declared on System.Type and has value type Type[]. [System.Array](https://learn.microsoft.com/en-us/dotnet/api/system.array?view=net-10.0) is the base of CLR arrays and derives Object; an array of Type does not inherit Type's static field.

The supported source correction is `[object].GetMethod('GetType',[Type]::EmptyTypes)`, using the actual empty Type[] argument. Do not invent a null result, runtime exception text or failed-child receipt for the unrun old line. Actual selected-runtime association and later execution stay separate gates. Preserve the source/parameter/result actions and actual selected empty-array owner in the fresh candidate ledgers; this finding grants no price or resource credit.

Root independently traced the complete static adapter route after the R13 census author raised the concern. The author of R14 has been instructed to fix only the new candidate, include a source regression/correspondence check and keep all prior evidence frozen. No Accepted ADR, cap, numerical threshold, Clock target, compiler, parser/AST/import, program, GUI, audio device or desktop input changed or ran for this finding.
