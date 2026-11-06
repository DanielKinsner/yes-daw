"""Evaluate the NEW ordered source ledgers, not any earlier census/helper.

Incoming argument coercions/default literals are caller-owned. Returned entries
here are source upper proposal components only, never selected prices or GO.
"""

def total(obj):
    if isinstance(obj,list): return sum(g['units'] for g in obj)
    return sum(obj.values())

def derive(m,I=1220):
    p={}
    fixed=lambda n:m[n]['fixed']
    alt=lambda n,k:total(m[n]['alternatives'][k])
    coef=lambda n,k:total(m[n]['coefficients'][k])
    p['Spend-Task']=fixed('Spend-Task')
    p['Test-TaskPathCatalogueContext']=fixed('Test-TaskPathCatalogueContext')
    p['Add-TaskPathCatalogueEntry']={k:fixed('Add-TaskPathCatalogueEntry')+alt('Add-TaskPathCatalogueEntry',k) for k in ('repo_input','ordinary_alias')}
    q=m['Initialize-TaskPathCatalogue']
    p['Initialize-TaskPathCatalogue']=q['fixed']+total(q['coefficients']['input_row'])*I+total(q['coefficients']['own_or_stage_row'])*(4+6)+119+total(q['coefficients']['descriptor_row'])*(56+3+4)
    p['Get-TaskCatalogueString']={k:fixed('Get-TaskCatalogueString')+alt('Get-TaskCatalogueString',k) for k in ('hit','miss')}
    p['Get-TaskCatalogueEntryPath']={k:fixed('Get-TaskCatalogueEntryPath')+alt('Get-TaskCatalogueEntryPath',k) for k in ('hit','tilde','first')}
    p['Get-TaskCanonicalPath']=fixed('Get-TaskCanonicalPath')
    n='Assert-NoLink';normal=fixed(n)+64*coef(n,'parent_body')
    p[n]=dict(normal=normal,depth65=normal+alt(n,'depth65_prefix')-alt(n,'replaced_normal_ending'),upper=max(normal,normal+alt(n,'depth65_prefix')-alt(n,'replaced_normal_ending')))
    p['Get-TaskHash']=dict(fixed=fixed('Get-TaskHash'),per_chunk=coef('Get-TaskHash','read_chunk'))
    p['Get-TaskHashHex']=fixed('Get-TaskHashHex')
    n='Test-TaskHashValue';p[n]=dict(hex=fixed(n)+alt(n,'hex')+32*coef(n,'hex_body'),binary=fixed(n)+alt(n,'binary')+32*coef(n,'binary_body'))
    n='Assert-TaskFreshHashIdentity';p[n]=dict(normal=fixed(n),refusal_body=alt(n,'refusal'),refusal_surplus=alt(n,'refusal')-1)
    n='Assert-TaskPollAdmission';p[n]=dict(normal=fixed(n),refusal=alt(n,'refusal'))
    n='Invoke-TaskChild';p[n]=dict(fixed=fixed(n),per_argument=coef(n,'argument_row'),per_string_unit=coef(n,'string_UTF16'),per_poll=coef(n,'normal_poll'),completion=coef(n,'completed_direction'),positive=coef(n,'positive_direction'))
    n='Add-TaskChildCleanup';p[n]=dict(fixed=fixed(n),per_Code_unit=coef(n,'Code_UTF16'),max_Code43=fixed(n)+43*coef(n,'Code_UTF16'))
    p['Set-TaskRawExit']=fixed('Set-TaskRawExit')
    n='Invoke-TaskFencedChild';p[n]=dict(fixed=fixed(n),binary=alt(n,'binary_additional'))
    n='Replace-TaskBounded';p[n]=dict(fixed=fixed(n),per_aggregate_argument_unit=coef(n,'all_argument_UTF16'),per_occurrence=coef(n,'IndexOf_body'))
    p['Reject-TaskGate']=fixed('Reject-TaskGate')
    n='Expect-TaskGate';p[n]=dict(fixed=fixed(n),per_expected_key=coef(n,'witness_key'))
    p['Add-TaskJsonVisit']=fixed('Add-TaskJsonVisit')
    p['Get-TaskRuntimeType']=fixed('Get-TaskRuntimeType')
    n='Get-TaskBootstrapHash';p[n]=dict(fixed=fixed(n),per_parent=coef(n,'parent'),per_chunk=coef(n,'read_chunk'))
    n='Initialize-TaskController';p[n]=dict(fixed=fixed(n),per_argument_unit=coef(n,'argument_UTF16'))
    n='Assert-TaskString';p[n]=dict(fixed=fixed(n),ASCII=coef(n,'ASCII_body'),general=coef(n,'general_surrogate_body'))
    n='Get-TaskSafePrefix';p[n]=dict(ASCII=fixed(n),general=fixed(n)+alt(n,'general_extra_decrement'))
    p['Assert-TaskIntegral']=fixed('Assert-TaskIntegral')
    n='Convert-TaskControlOperand';p[n]={k:fixed(n)+alt(n,k) for k in ('builtin','biginteger')}
    n='Assert-TaskExactKeys';p[n]=dict(fixed=fixed(n),per_key=coef(n,'declared_key')+coef(n,'observed_key'),per_actual_key_unit=coef(n,'key_UTF16'))
    p['Set-TaskSecondary']=fixed('Set-TaskSecondary')
    for n in ('Close-TaskOwner','Close-TaskJsonReader'):
        p[n]={k:fixed(n)+alt(n,k) for k in ('null','normal','firstFailure','secondaryFailure')}
        p[n]['upper']=max(p[n].values())
    p['Complete-TaskOwnerClose']=dict(normal=fixed('Complete-TaskOwnerClose'),propagate=alt('Complete-TaskOwnerClose','propagate'))
    p['Get-TaskSecondaryCodes']=fixed('Get-TaskSecondaryCodes')
    p['Assert-TaskCodecBinding']=dict(normal=fixed('Assert-TaskCodecBinding'),refusal=alt('Assert-TaskCodecBinding','default_refusal'))
    n='Get-TaskIntegerDigits';p[n]=dict(negative=fixed(n)+18*13,unsigned=alt(n,'unsigned_fixed')+19*12)
    n='Get-TaskEscapedStringBytes';p[n]=dict(fixed=fixed(n),per_unit=coef(n,'UTF16'))
    p['Add-TaskJsonBound']=fixed('Add-TaskJsonBound')
    n='Visit-TaskJsonValue'
    p[n]={k:alt(n,k) for k in ('string','boolean','integer','map_fixed','array_fixed')}
    p[n].update(null=fixed(n)+alt(n,'null'),string_per_unit=148,map_per_entry=coef(n,'map_entry'),map_per_key_unit=coef(n,'map_key_UTF16'),map_separator=coef(n,'map_separator'),array_per_entry=coef(n,'array_entry'),array_separator=coef(n,'array_separator'))
    p['Measure-TaskJson']=fixed('Measure-TaskJson')
    n='Join-TaskBoundedNames';p[n]=dict(fixed=fixed(n),per_name=coef(n,'name_row'),per_name_unit=coef(n,'name_UTF16'))
    n='Get-TaskEntries';p[n]=dict(fixed=fixed(n),per_directory=coef(n,'directory'),per_candidate=coef(n,'candidate'),per_path_unit=coef(n,'candidate_UTF16'),overflow=coef(n,'overflow'),replaced_ending=alt(n,'replaced_normal_ending'))
    n='Copy-TaskOwned';p[n]=dict(fixed=fixed(n),per_chunk=coef(n,'chunk'))
    p['Mutate-TaskOwnedBytes']=fixed('Mutate-TaskOwnedBytes')
    for n in ('Assert-TaskRawJson','Assert-TaskRawJsonBytes'):
        classes={k:coef(n,k) for k in ('outer_integer_path','string_body','hex_body','digits_body','magnitude_body','literal_body')}
        grammar=sum(classes.values());failure=alt(n,'first_failure_tail')
        if n.endswith('Bytes'):
            p[n]=dict(classes=classes,grammar=grammar,common=fixed(n),failure=failure,compaction=coef(n,'compaction'),compaction_fixed=alt(n,'compaction_fixed'),per_input_upper=grammar+coef(n,'compaction'),fixed_upper=fixed(n)+max(failure,alt(n,'compaction_fixed')))
        else:p[n]=dict(classes=classes,per_input_upper=grammar,fixed_upper=fixed(n)+max(failure,alt(n,'normal_return')))
    p['Read-TaskBoundedBytes']=fixed('Read-TaskBoundedBytes')
    p['Read-TaskDependencyJson']=fixed('Read-TaskDependencyJson')
    n='Get-TaskDependencyString';p[n]=dict(fixed=fixed(n),per_unit=coef(n,'UTF16'))
    n='Read-TaskJson';p[n]={k:fixed(n)+alt(n,k) for k in ('dependency','predecessor')}
    n='Encode-TaskJson';p[n]=dict(fixed=fixed(n),per_preLF_unit=coef(n,'output_UTF16'))
    n='Get-TaskStageBytes';p[n]=dict(fixed=fixed(n),per_row=coef(n,'row'))
    n='Assert-TaskDocumentWrite';p[n]=dict(fixed=fixed(n),per_path_unit=coef(n,'Path_UTF16'))
    p['Write-TaskJsonBytes']=fixed('Write-TaskJsonBytes')
    p['Write-TaskJson']=dict(measure_null=fixed('Write-TaskJson'),measurement_supplied=fixed('Write-TaskJson')-4)
    p['Save-TaskReceipt']=fixed('Save-TaskReceipt')
    n='Assert-TaskIdentity';p[n]=dict(normal=fixed(n),refusal=fixed(n)+alt(n,'refusal_surplus'))
    n='Assert-TaskBoundSourceInteger';p[n]=dict(normal=fixed(n),kind=fixed(n)+alt(n,'kind_surplus'),nonpositive=fixed(n)+alt(n,'nonpositive_surplus'))
    for n in ('Assert-TaskReservation','Assert-TaskRetainedReservation','Assert-TaskAttempt','Assert-TaskConsolePE','Assert-TaskPredecessor'):
        p[n]=dict(normal=fixed(n),refusal=fixed(n)+alt(n,'refusal_surplus'))
    p['Get-TaskPredecessorDescriptor']=fixed('Get-TaskPredecessorDescriptor')
    n='Resolve-TaskInput';p[n]=dict(fixed=fixed(n),per_unit=coef(n,'Name_UTF16'))
    n='Assert-TaskPins';p[n]=dict(fixed=fixed(n),per_namespace=coef(n,'namespace_row'),per_callback_pair=coef(n,'Where_callback')+coef(n,'projection_callback'),per_own=coef(n,'own4_row'),per_input=coef(n,'input_row'))
    n='Assert-TaskFileSet';p[n]=dict(fixed=fixed(n),expected=coef(n,'expected_declared_row')+coef(n,'expected_missing_scan'),observed=coef(n,'observed_row'),per_name_unit=coef(n,'string_UTF16'),refusal_fixed=fixed(n)+alt(n,'refusal_surplus'),refusal_expected=coef(n,'expected_declared_row')+coef(n,'expected_missing_scan')+coef(n,'refusal_missing_extra'),refusal_observed=coef(n,'observed_row')+coef(n,'refusal_observed_extra'))
    n='Get-TaskReferences';p[n]=dict(fixed=fixed(n),per_entry=coef(n,'common_entry'),per_file=coef(n,'file'))
    n='Assert-TaskSameReferences';p[n]=dict(fixed=fixed(n),per_file=coef(n,'before_row')+coef(n,'after_row'))
    p['Get-TaskPEHeader']=fixed('Get-TaskPEHeader')
    n='Read-TaskChildResult';p[n]=dict(fixed=fixed(n),per_text_unit=coef(n,'scan_UTF16'),per_control=coef(n,'admitted_control'),bad_ID_prefix=alt(n,'first_bad_ID_prefix'))
    n='Get-TaskBuildArtifacts';p[n]=dict(fixed=fixed(n),per_entry=coef(n,'file_row'))
    n='Assert-TaskDependencies';p[n]=dict(fixed=fixed(n),per_include=coef(n,'include_row'),one_string_fixed=coef(n,'extracted_string_fixed'),per_string_unit=coef(n,'string_UTF16'))
    n='Save-TaskFailureInventory';p[n]=dict(fixed=fixed(n),per_file=coef(n,'file_row'))
    assert set(p)==set(m),(set(m)-set(p),set(p)-set(m))
    return p
