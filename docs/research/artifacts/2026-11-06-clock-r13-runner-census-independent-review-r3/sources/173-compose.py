"""Fresh complete54-family composition of current source-derived body groups.

No old census/oracle is imported or executed. Caller-only main input is DATA.
"""
def derive(p,schedule,graphs,main):
    def visitor(g):
        q=p['Visit-TaskJsonValue']
        return (q['map_fixed']*g.get('maps',0)+q['array_fixed']*g.get('arrays',0)
          +q['map_per_entry']*g.get('keys',0)+q['map_separator']*g.get('map_separators',0)
          +q['map_per_key_unit']*g.get('key_units',0)+q['array_per_entry']*g.get('array_elements',0)
          +q['array_separator']*g.get('array_separators',0)+q['string']*g.get('strings',0)
          +q['string_per_unit']*g.get('string_units',0)+q['integer']*g.get('integers',0)
          +q['boolean']*g.get('booleans',0))
    def json_upper(g):
        # ASCII literal key identifiers, arbitrary admitted valid value strings.
        return (2*(g.get('maps',0)+g.get('arrays',0))+g.get('keys',0)
          +g.get('map_separators',0)+g.get('array_separators',0)
          +g.get('key_units',0)+2*g.get('keys',0)
          +6*g.get('string_units',0)+2*g.get('strings',0)
          +20*g.get('integers',0)+5*g.get('booleans',0)+1)
    for g in graphs.values():
        g['visitor_upper']=visitor(g);g['wire_upper_including_LF']=json_upper(g)
    # Marker values are fixed ASCII;6x escape is not needed for its75units.
    marker=graphs['marker'];marker['wire_upper_including_LF']-=5*marker['string_units']
    def join(n,u):
        q=p['Join-TaskBoundedNames'];return q['fixed']+q['per_name']*n+q['per_name_unit']*u
    def refusal(n,o,missing,unknown):
        q=p['Assert-TaskFileSet']
        return (q['refusal_fixed']+q['refusal_expected']*n+q['refusal_observed']*o
          +q['per_name_unit']*512*(n+o)+join(len(missing),sum(map(len,missing)))
          +join(len(unknown),sum(map(len,unknown))))
    reference_controls=sum([refusal(56,55,['references/native-inverse/r48000-map5.json'],[]),
      refusal(56,57,[],['unlisted.bin']),refusal(56,57,[],[])])
    # Fixed32-byte hashes, original7 runtime pins use source lengths379chunks.
    ctrl=p['Initialize-TaskController'];bh=p['Get-TaskBootstrapHash']
    controller=(ctrl['fixed']+ctrl['per_argument_unit']*14*512
      +7*(bh['fixed']+64*bh['per_parent'])+379*bh['per_chunk'])
    #84 calls:28 actual argument slots times tokens<BASE>/<REPO>/<BUILD>.
    # No assumption that a replacement/root cannot contain a later token.
    rp=p['Replace-TaskBounded'];tokens=[6,6,7]
    replacement=dict(calls=28*len(tokens),aggregate_argument_units=2*84*512+28*sum(tokens),
                     match_bodies=28*sum(512//n for n in tokens),not_found_prefixes=84)
    replacement['upper']=(rp['fixed']*84+rp['per_aggregate_argument_unit']*replacement['aggregate_argument_units']
      +rp['per_occurrence']*replacement['match_bodies']+18*replacement['not_found_prefixes'])
    # Each generic failed FileSet replaces an existing slot. Only fixed102 is
    # guaranteed sponsored there; never subtract an uncharged maximum-N price.
    sf=p['Assert-TaskFileSet'];facts=schedule['facts']
    n=facts['generic_FileSet_expected_max'];o=facts['generic_FileSet_observed_max']
    generic_refusal=(sf['refusal_fixed']+sf['refusal_expected']*n+sf['refusal_observed']*o
      +sf['per_name_unit']*512*(n+o)+join(n,512*n)+join(o,512*o))
    assert generic_refusal==307+65713*n+65710*o
    # Ordered expected control witness sizes from actual ten source calls.
    witness_keys=[2,3,3,3,2,5,4,5,5,5]
    phases=[]
    for a in schedule['phases']:
        f=a['fences'];c=a['children'];r=a['references'];b=a['builds'];made=a['created'];dep=a['postcompiler'];prod=a['postproducer'];verified=a['postverify'];bundle=a['bundle']
        S=a['stage'];W=a['walkers'];D=a['directories'];E=a['normal_rows']
        expect_count=6*made+dep+3*prod
        expect_keys=sum(witness_keys[:6])*made+witness_keys[6]*dep+sum(witness_keys[7:])*prod
        expected_own=p['Expect-TaskGate']['fixed']*expect_count+p['Expect-TaskGate']['per_expected_key']*expect_keys
        arg_count=sum([28,6,6][:c]);q=p['Invoke-TaskChild']
        one_string=p['Assert-TaskString']['fixed']+512*p['Assert-TaskString']['ASCII']
        encode_caps=dict(marker=graphs['marker']['wire_upper_including_LF'],receipt=65536,
          failure=min(1048576,graphs['failure']['wire_upper_including_LF']),bundle=1048576)
        encoded_preLF_units=(a['markers']*(encode_caps['marker']-1)+a['receipts']*(encode_caps['receipt']-1)
           +a['failures']*(encode_caps['failure']-1)+bundle*(encode_caps['bundle']-1))
        C={
          'bootstrapDefinitionsAll74Registrations':main['bootstrap_registration_units'],
          'wholeMainCallerAndCatchSourcePositivePrefix':main['main_units'],
          'controllerAndSevenFixedAuthenticatedHashes':controller,
          'freshSHAOwnBodiesExcludingNoLink':p['Get-TaskHash']['fixed']*a['H']+p['Get-TaskHash']['per_chunk']*a['hash_chunks'],
          'allNoLinkOwnBodies':p['Assert-NoLink']['upper']*a['NoLink'],
          'allResolveOwnBodies':(p['Resolve-TaskInput']['fixed']+512*p['Resolve-TaskInput']['per_unit'])*a['Resolve'],
          'allWalkerOwnBodiesAndOverflowAlternatives':p['Get-TaskEntries']['fixed']*W+p['Get-TaskEntries']['per_directory']*D+(p['Get-TaskEntries']['per_candidate']+512*p['Get-TaskEntries']['per_path_unit'])*E+max(0,p['Get-TaskEntries']['overflow']+512*p['Get-TaskEntries']['per_path_unit']-p['Get-TaskEntries']['replaced_ending'])*W,
          'allStageByteOwnBodies':p['Get-TaskStageBytes']['fixed']*S+p['Get-TaskStageBytes']['per_row']*(6*S+(3 if S else 0)),
          'allPinsOwnCallerBodies':f*(p['Assert-TaskPins']['fixed']+p['Assert-TaskPins']['per_namespace']*facts['namespace_count']+p['Assert-TaskPins']['per_callback_pair']*facts['namespace_guard_rows']+p['Assert-TaskPins']['per_own']*4+p['Assert-TaskPins']['per_input']*facts['input_count']),
          'normalFileSetSlots':sf['fixed']*a['FileSet_slots']+(sf['expected']+sf['observed']+2*sf['per_name_unit']*512)*a['FileSet_success_elements_upper'],
          'twoFileSetRefusalSlotReplacements':2*(generic_refusal-sf['fixed']),
          'expectedControlsOwnBodiesAndThreeReferenceRefusals':expected_own+prod*reference_controls,
          'childOwnAllPollsAnd259OuterReads':c*q['fixed']+q['per_argument']*arg_count+q['per_string_unit']*512*(arg_count+8*c),
          'fencedWrapperOwnCallers':c*p['Invoke-TaskFencedChild']['fixed']+a['native_parses']*p['Invoke-TaskFencedChild']['binary'],
          'compilerReplacementHelpers84':int(c>0)*replacement['upper'],
          'twoOwnedCopyBodies':a['copies']*p['Copy-TaskOwned']['fixed']+p['Copy-TaskOwned']['per_chunk']*(0 if not a['copies'] else (2 if a['copies']==1 else 514)),
          'ownedMutationBodies':p['Mutate-TaskOwnedBytes']*a['copies'],
          'PEHeaderBodies':4*dep*p['Get-TaskPEHeader'],
          'ordinaryConsoleChecksAndActualControlRefusal':int(c>0)*p['Assert-TaskConsolePE']['normal']+dep*(2*p['Assert-TaskConsolePE']['normal']+p['Assert-TaskConsolePE']['refusal']),
          'predecessorDescriptor':p['Get-TaskPredecessorDescriptor'],
          'predecessorMap':p['Assert-TaskPredecessor']['refusal'],
          'boundSourceSignedIntegers':4*max(p['Assert-TaskBoundSourceInteger'].values()),
          'workReservationNormalAndActualRefusal':p['Assert-TaskReservation']['normal']*(1+made)+p['Assert-TaskReservation']['refusal']*made,
          'retainedReservationNormalAndActualRefusal':p['Assert-TaskRetainedReservation']['normal']*(1+bundle)+p['Assert-TaskRetainedReservation']['refusal']*made,
          'oneUseAttemptNormalAndActualRefusal':p['Assert-TaskAttempt']['normal']+p['Assert-TaskAttempt']['refusal']*made,
          'lastLegalAndOnePastPollBoundary':made*(p['Assert-TaskPollAdmission']['normal']+p['Assert-TaskPollAdmission']['refusal']),
          'rootAsciiValidation':one_string,
          'compilerArgumentAsciiValidation':int(c>0)*28*one_string,
          'allMeasureOwn':p['Measure-TaskJson']*a['Measure'],
          'allRecursiveVisitors':a['markers']*visitor(graphs['marker'])+a['receipts']*visitor(graphs['receipt'])+a['failures']*visitor(graphs['failure'])+a['bundle_passes']*visitor(graphs['bundle']),
          'receiptOwnAndSecondaryWithoutMeasure':a['receipts']*(p['Save-TaskReceipt']+p['Get-TaskSecondaryCodes']),
          'WriteJsonOwnWithoutMeasure':p['Write-TaskJson']['measure_null']*a['failures']+p['Write-TaskJson']['measurement_supplied']*bundle,
          'allDocumentWriteOwn':a['DocumentWrite']*(p['Assert-TaskDocumentWrite']['fixed']+512*p['Assert-TaskDocumentWrite']['per_path_unit']),
          'allOnceEncodeOwn':p['Encode-TaskJson']['fixed']*a['Encode']+p['Encode-TaskJson']['per_preLF_unit']*encoded_preLF_units,
          'bootstrapBoundedRead':p['Read-TaskBoundedBytes'],
          'predecessorAndDependencyReadJsonOwn':p['Read-TaskJson']['predecessor']+dep*p['Read-TaskJson']['dependency'],
          'streamedDependencyReaderAllOwners':dep*p['Read-TaskDependencyJson'],
          'dependencyExtractionAndStringGetters':dep*(p['Assert-TaskDependencies']['fixed']+p['Get-TaskDependencyString']['fixed']+512*p['Get-TaskDependencyString']['per_unit']+1221*(p['Assert-TaskDependencies']['per_include']+p['Get-TaskDependencyString']['fixed']+512*p['Get-TaskDependencyString']['per_unit'])),
          'nativeResultReaderOwnAnd14Controls':a['native_parses']*p['Read-TaskChildResult']['fixed']+p['Read-TaskChildResult']['per_text_unit']*a['raw_string_units']+a['actual_module_controls']*p['Read-TaskChildResult']['per_control'],
          'oneBadControlPrefixReplacement':int(a['native_parses']>0)*p['Read-TaskChildResult']['bad_ID_prefix'],
          'buildArtifactOwnWithoutHashHex':b*(p['Get-TaskBuildArtifacts']['fixed']+4*p['Get-TaskBuildArtifacts']['per_entry']),
          'referenceOwnWithoutHashHex':r*(p['Get-TaskReferences']['fixed']+60*p['Get-TaskReferences']['per_entry']+60*p['Get-TaskReferences']['per_file']),
          'sameReferenceDescriptorIdentities':verified*(p['Assert-TaskSameReferences']['fixed']+56*p['Assert-TaskSameReferences']['per_file']),
          'finalBuildDescriptorIdentityBodies':verified*3*p['Assert-TaskIdentity']['normal'],
          'twoDescriptorIdentityRefusalSlotReplacements':2*(p['Assert-TaskIdentity']['refusal']-p['Assert-TaskIdentity']['normal']),
          'allFreshIdentityOwnWithoutTests':a['Fresh_normal']*p['Assert-TaskFreshHashIdentity']['normal']+a['Fresh_refusals']*p['Assert-TaskFreshHashIdentity']['refusal_surplus'],
          'allNativeDigestHexAndBinaryTests':a['hex_tests']*p['Test-TaskHashValue']['hex']+a['binary_tests']*p['Test-TaskHashValue']['binary'],
          'wireHexWrapperBodies':a['wire_hex']*p['Get-TaskHashHex'],
          'failureInventoryOwn':a['failures']*(p['Save-TaskFailureInventory']['fixed']+78*p['Save-TaskFailureInventory']['per_file']),
          'allRawByteGrammarAndCompaction':p['Assert-TaskRawJsonBytes']['per_input_upper']*a['raw_byte_units']+p['Assert-TaskRawJsonBytes']['fixed_upper']*a['raw_byte_calls'],
          'allRawStringGrammars':p['Assert-TaskRawJson']['per_input_upper']*a['raw_string_units']+p['Assert-TaskRawJson']['fixed_upper']*a['raw_string_calls'],
          'allCanonicalOwn':p['Get-TaskCanonicalPath']*a['Canonical'],
          'allEntryPathOwn':p['Get-TaskCatalogueEntryPath']['tilde']*a['EntryPath']+(p['Get-TaskCatalogueEntryPath']['first']-p['Get-TaskCatalogueEntryPath']['tilde'])*a['firstBuild_upper'],
          'initializeCatalogue':p['Initialize-TaskPathCatalogue']}
        assert len(C)==54,len(C)
        phases.append(dict(id=a['id'],profile=a,composition=C,upper=sum(C.values()),
          expected_control_calls=expect_count,expected_witness_keys=expect_keys,encode_cap_units=encode_caps))
    return dict(phases=phases,upper=max(x['upper'] for x in phases),graphs=graphs,
       generic_FileSet_refusal=dict(expected_max=n,observed_max=o,whole=generic_refusal,replacement_subtraction_only_fixed=sf['fixed'],two_replacements=2*(generic_refusal-sf['fixed'])),
       reference_controls_upper=reference_controls,replacement_helpers=replacement,
       controller_upper=controller,trace_claimed=False)
