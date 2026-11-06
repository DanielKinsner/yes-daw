"""New54-family/six-prefix integer composition, no old code/price execution."""
def compose_all(p,schedule,graphs,main):
    def visitor(g):
        q=p['Visit-TaskJsonValue']
        return sum([q['map_fixed']*g.get('maps',0),q['array_fixed']*g.get('arrays',0),
          q['map_per_entry']*g.get('keys',0),q['map_per_key_unit']*g.get('key_units',0),q['map_separator']*g.get('map_separators',0),
          q['array_per_entry']*g.get('array_elements',0),q['array_separator']*g.get('array_separators',0),
          q['string']*g.get('strings',0),q['string_per_unit']*g.get('string_units',0),q['integer']*g.get('integers',0),q['boolean']*g.get('booleans',0)])
    for key,g in graphs.items():
        g['visitor_upper']=visitor(g)
        g['wire_upper_including_LF']=(2*(g.get('maps',0)+g.get('arrays',0))+3*g.get('keys',0)+g.get('map_separators',0)+g.get('array_separators',0)
          +g.get('key_units',0)+6*g.get('string_units',0)+2*g.get('strings',0)+20*g.get('integers',0)+5*g.get('booleans',0)+1)
        if key=='marker':g['wire_upper_including_LF']-=5*g['string_units']
    def join(n,u):
        q=p['Join-TaskBoundedNames'];return q['fixed']+q['per_name']*n+q['per_name_unit']*u
    sf=p['Assert-TaskFileSet'];facts=schedule['facts']
    def refuse(n,o,missing,unknown):
        return sf['refusal_fixed']+sf['refusal_expected']*n+sf['refusal_observed']*o+sf['per_name_unit']*512*(n+o)+join(len(missing),sum(map(len,missing)))+join(len(unknown),sum(map(len,unknown)))
    refcontrols=refuse(56,55,['references/native-inverse/r48000-map5.json'],[])+refuse(56,57,[],['unlisted.bin'])+refuse(56,57,[],[])
    n,o=facts['generic_FileSet_expected_max'],facts['generic_FileSet_observed_max']
    failedset=sf['refusal_fixed']+sf['refusal_expected']*n+sf['refusal_observed']*o+sf['per_name_unit']*512*(n+o)+join(n,512*n)+join(o,512*o)
    ctrl,bh=p['Initialize-TaskController'],p['Get-TaskBootstrapHash']
    controller=ctrl['fixed']+ctrl['per_argument_unit']*14*512+7*(bh['fixed']+64*bh['per_parent'])+379*bh['per_chunk']
    rp=p['Replace-TaskBounded'];tokens=[len('<BASE>'),len('<REPO>'),len('<BUILD>')]
    replacement={'calls':28*3,'aggregate_argument_units':2*84*512+28*sum(tokens),'match_bodies':28*sum(512//t for t in tokens),'not_found_prefixes':84}
    replacement['upper']=84*rp['fixed']+rp['per_aggregate_argument_unit']*replacement['aggregate_argument_units']+rp['per_occurrence']*replacement['match_bodies']+18*84
    witnesses=[2,3,3,3,2,5,4,5,5,5]
    phases=[]
    for a in schedule['phases']:
        f,c,r,b,S=a['fences'],a['children'],a['references'],a['builds'],a['stage']
        made,dep,prod,verified,bundle=a['created'],a['postcompiler'],a['postproducer'],a['postverify'],a['bundle']
        W,D,E=a['walkers'],a['directories'],a['normal_rows']
        expectedcalls=6*made+dep+3*prod
        expectedkeys=sum(witnesses[:6])*made+witnesses[6]*dep+sum(witnesses[7:])*prod
        expect=p['Expect-TaskGate']['fixed']*expectedcalls+p['Expect-TaskGate']['per_expected_key']*expectedkeys
        ascii512=p['Assert-TaskString']['fixed']+512*p['Assert-TaskString']['ASCII']
        argslots=sum([28,6,6][:c]);child=p['Invoke-TaskChild']
        caps={'marker':graphs['marker']['wire_upper_including_LF'],'receipt':65536,'failure':min(1048576,graphs['failure']['wire_upper_including_LF']),'bundle':1048576}
        encoded_units=a['markers']*(caps['marker']-1)+a['receipts']*(caps['receipt']-1)+a['failures']*(caps['failure']-1)+bundle*(caps['bundle']-1)
        C={};equations={}
        def term(name,value,equation):
            assert name not in C and type(value) is int and value>=0
            C[name]=value;equations[name]=equation
        term('bootstrapDefinitionsAll74Registrations',main['bootstrap_registration_units'],'root.mainR3.bootstrap_registration_units=51125, caller-only data; 74 current bodies composed separately')
        term('wholeMainCallerAndCatchSourcePositivePrefix',main['main_units'],'root.mainR3.main_units=129674, caller/callback/handler-only positive envelope')
        term('controllerAndSevenFixedAuthenticatedHashes',controller,'controller.fixed + controller.argumentUnit*14*512 + 7*(bootstrap.fixed+64*bootstrap.parent) +379*bootstrap.chunk')
        term('freshSHAOwnBodiesExcludingNoLink',p['Get-TaskHash']['fixed']*a['H']+p['Get-TaskHash']['per_chunk']*a['hash_chunks'],'Hash.fixed*H + Hash.chunk*hash_chunks; actual caller coercions are not duplicated here')
        term('allNoLinkOwnBodies',p['Assert-NoLink']['upper']*a['NoLink'],'NoLink.depth65_upper*NoLink_calls; Canonical separate')
        term('allResolveOwnBodies',(p['Resolve-TaskInput']['fixed']+512*p['Resolve-TaskInput']['per_unit'])*a['Resolve'],'(Resolve.fixed+512*Resolve.unit)*Resolve_calls; Entry separate')
        term('allWalkerOwnBodiesAndOverflowAlternatives',p['Get-TaskEntries']['fixed']*W+p['Get-TaskEntries']['per_directory']*D+(p['Get-TaskEntries']['per_candidate']+512*p['Get-TaskEntries']['per_path_unit'])*E+max(0,p['Get-TaskEntries']['overflow']+512*p['Get-TaskEntries']['per_path_unit']-p['Get-TaskEntries']['replaced_ending'])*W,'Entries.fixed*W + Entries.directory*D +(Entries.candidate+512*unit)*E + max(0,overflow+512*unit-replaced_ending)*W')
        term('allStageByteOwnBodies',p['Get-TaskStageBytes']['fixed']*S+p['Get-TaskStageBytes']['per_row']*(6*S+(3 if S else 0)),'Stage.fixed*S + Stage.row*(6*S+3 if S>0 else 0); three additional first-failing captured rows')
        term('allPinsOwnCallerBodies',f*(p['Assert-TaskPins']['fixed']+p['Assert-TaskPins']['per_namespace']*facts['namespace_count']+p['Assert-TaskPins']['per_callback_pair']*facts['namespace_guard_rows']+p['Assert-TaskPins']['per_own']*4+p['Assert-TaskPins']['per_input']*facts['input_count']),'f*(Pins.fixed+Pins.namespace*31+Pins.callbackPair*1123+Pins.own*4+Pins.input*1220)')
        term('normalFileSetSlots',sf['fixed']*a['FileSet_slots']+(sf['expected']+sf['observed']+2*sf['per_name_unit']*512)*a['FileSet_success_elements_upper'],'FileSet.fixed*slots+(expected+observed+2*512*unit)*success_elements_upper')
        term('twoFileSetRefusalSlotReplacements',2*(failedset-sf['fixed']),'2*(complete_generic_refusal - guaranteed_sponsored_fixed102), not a refund of uncharged max-N normal work')
        term('expectedControlsOwnBodiesAndThreeReferenceRefusals',expect+prod*refcontrols,'Expect.fixed*expectedcalls+Expect.key*expectedkeys + postproducer*three_exact_reference_refusals')
        term('childOwnAllPollsAnd259OuterReads',c*child['fixed']+child['per_argument']*argslots+child['per_string_unit']*512*(argslots+8*c),'Child.fixed*c + Child.argument*sum([28,6,6][:c]) + ASCIIunit*512*(arguments+8*c); fixed expands7200polls/259outerreads')
        term('fencedWrapperOwnCallers',c*p['Invoke-TaskFencedChild']['fixed']+a['native_parses']*p['Invoke-TaskFencedChild']['binary'],'Fenced.fixed*c + native_parses*Fenced.binary (caller-only transitive exclusions explicit)')
        term('compilerReplacementHelpers84',int(c>0)*replacement['upper'],'child_started_prefix*(84*Replace.fixed+Replace.unit*86548+Replace.match*6804+84*18)')
        term('twoOwnedCopyBodies',a['copies']*p['Copy-TaskOwned']['fixed']+p['Copy-TaskOwned']['per_chunk']*(0 if not a['copies'] else 2 if a['copies']==1 else 514),'Copy.fixed*copies+Copy.chunk*(0/2/514); own buffers/storage not a scalar fee')
        term('ownedMutationBodies',p['Mutate-TaskOwnedBytes']*a['copies'],'Mutate.complete*copies')
        term('PEHeaderBodies',4*dep*p['Get-TaskPEHeader'],'4*postcompiler*PE.complete')
        term('ordinaryConsoleChecksAndActualControlRefusal',int(c>0)*p['Assert-TaskConsolePE']['normal']+dep*(2*p['Assert-TaskConsolePE']['normal']+p['Assert-TaskConsolePE']['refusal']),'prelaunch compiler normal + postcompiler*(two normal + actual GUI refusal)')
        term('predecessorDescriptor',p['Get-TaskPredecessorDescriptor'],'PredecessorDescriptor.complete once (includes exact own subhelpers declared there)')
        term('predecessorMap',p['Assert-TaskPredecessor']['refusal'],'Predecessor.complete refusal upper once')
        term('boundSourceSignedIntegers',4*max(p['Assert-TaskBoundSourceInteger'].values()),'4*max(BoundSigned.normal,kind_refusal,nonpositive_refusal)')
        term('workReservationNormalAndActualRefusal',p['Assert-TaskReservation']['normal']*(1+made)+p['Assert-TaskReservation']['refusal']*made,'Reservation.normal*(1+created)+Reservation.refusal*created')
        term('retainedReservationNormalAndActualRefusal',p['Assert-TaskRetainedReservation']['normal']*(1+bundle)+p['Assert-TaskRetainedReservation']['refusal']*made,'Retained.normal*(1+bundle)+Retained.refusal*created')
        term('oneUseAttemptNormalAndActualRefusal',p['Assert-TaskAttempt']['normal']+p['Assert-TaskAttempt']['refusal']*made,'Attempt.normal + Attempt.refusal*created')
        term('lastLegalAndOnePastPollBoundary',made*(p['Assert-TaskPollAdmission']['normal']+p['Assert-TaskPollAdmission']['refusal']),'created*(Poll.normal+Poll.refusal); actual control is separate from normal child polls')
        term('rootAsciiValidation',ascii512,'String.fixed+512*String.ASCII')
        term('compilerArgumentAsciiValidation',int(c>0)*28*ascii512,'compiler_prefix*28*(String.fixed+512*String.ASCII); main compiler argument validation distinct from child validation')
        term('allMeasureOwn',p['Measure-TaskJson']*a['Measure'],'Measure.complete*Measure_calls; graph recursive visits separate')
        term('allRecursiveVisitors',a['markers']*visitor(graphs['marker'])+a['receipts']*visitor(graphs['receipt'])+a['failures']*visitor(graphs['failure'])+a['bundle_passes']*visitor(graphs['bundle']),'markers*visit(marker)+receipts*visit(receipt)+failures*visit(failure)+bundle_passes*visit(bundle)')
        term('receiptOwnAndSecondaryWithoutMeasure',a['receipts']*(p['Save-TaskReceipt']+p['Get-TaskSecondaryCodes']),'receipts*(Receipt.own+SecondaryCodes.complete)')
        term('WriteJsonOwnWithoutMeasure',p['Write-TaskJson']['measure_null']*a['failures']+p['Write-TaskJson']['measurement_supplied']*bundle,'failure*WriteJson.nullMeasurement + bundle*WriteJson.suppliedMeasurement; omitted default coercion is FailureInventory caller')
        term('allDocumentWriteOwn',a['DocumentWrite']*(p['Assert-TaskDocumentWrite']['fixed']+512*p['Assert-TaskDocumentWrite']['per_path_unit']),'DocumentWrite_calls*(Document.fixed+512*Document.unit)')
        term('allOnceEncodeOwn',p['Encode-TaskJson']['fixed']*a['Encode']+p['Encode-TaskJson']['per_preLF_unit']*encoded_units,'Encode.fixed*Encode_calls+Encode.unit*sum(document_multiplicity*(preflight_admitted_cap-1))')
        term('bootstrapBoundedRead',p['Read-TaskBoundedBytes'],'ReadBounded.identity; actual main literal-long lift belongs to root caller-only component')
        term('predecessorAndDependencyReadJsonOwn',p['Read-TaskJson']['predecessor']+dep*p['Read-TaskJson']['dependency'],'ReadJson.predecessor+postcompiler*ReadJson.dependency; incoming coercions in actual callers')
        term('streamedDependencyReaderAllOwners',dep*p['Read-TaskDependencyJson'],'postcompiler*ReadDependency.complete')
        ds=p['Get-TaskDependencyString'];dp=p['Assert-TaskDependencies']
        term('dependencyExtractionAndStringGetters',dep*(dp['fixed']+ds['fixed']+512*ds['per_unit']+1221*(dp['per_include']+ds['fixed']+512*ds['per_unit'])),'postcompiler*(Dependencies.fixed+oneSourceString +1221*(Dependencies.include+oneIncludeString)); Resolve separate')
        q=p['Read-TaskChildResult']
        term('nativeResultReaderOwnAnd14Controls',a['native_parses']*q['fixed']+q['per_text_unit']*a['raw_string_units']+a['actual_module_controls']*q['per_control'],'native_parses*ReadChild.fixed + scanUnit*raw_string_units + admittedControls*ReadChild.control')
        term('oneBadControlPrefixReplacement',int(a['native_parses']>0)*q['bad_ID_prefix'],'one first-bad-ID source prefix replaces later summary Raw work; complete source-coupled proof in argument')
        term('buildArtifactOwnWithoutHashHex',b*(p['Get-TaskBuildArtifacts']['fixed']+4*p['Get-TaskBuildArtifacts']['per_entry']),'build_captures*(Build.fixed+4*Build.file); exact successful returned descriptor set remains3')
        term('referenceOwnWithoutHashHex',r*(p['Get-TaskReferences']['fixed']+60*p['Get-TaskReferences']['per_entry']+60*p['Get-TaskReferences']['per_file']),'reference_captures*(References.fixed+60*References.entry+60*References.file); hashes before pool/exact-set refusal retained')
        term('sameReferenceDescriptorIdentities',verified*(p['Assert-TaskSameReferences']['fixed']+56*p['Assert-TaskSameReferences']['per_file']),'postverify*(SameReferences.fixed+56*SameReferences.file)')
        term('finalBuildDescriptorIdentityBodies',verified*3*p['Assert-TaskIdentity']['normal'],'postverify*3*Identity.normal')
        term('twoDescriptorIdentityRefusalSlotReplacements',2*(p['Assert-TaskIdentity']['refusal']-p['Assert-TaskIdentity']['normal']),'two actual identity slots replace their own normal return only with complete witness/Reject tail')
        fi=p['Assert-TaskFreshHashIdentity']
        term('allFreshIdentityOwnWithoutTests',a['Fresh_normal']*fi['normal']+a['Fresh_refusals']*fi['refusal_surplus'],'Fresh_normal*Fresh.normal+Fresh_refusals*(complete_refusal_tail-replaced_own_T1); up to4 refusals')
        term('allNativeDigestHexAndBinaryTests',a['hex_tests']*p['Test-TaskHashValue']['hex']+a['binary_tests']*p['Test-TaskHashValue']['binary'],'hexTests*Test.hex+binaryTests*Test.binary; opaque bulk hash CPU not invented')
        term('wireHexWrapperBodies',a['wire_hex']*p['Get-TaskHashHex'],'wire_hex_calls*HashHex.complete; no Tuple/raw byte wire values')
        term('failureInventoryOwn',a['failures']*(p['Save-TaskFailureInventory']['fixed']+78*p['Save-TaskFailureInventory']['per_file']),'failures*(FailureInventory.fixed+78*FailureInventory.file); includes default literalR and longO distinct from calleeW')
        q=p['Assert-TaskRawJsonBytes'];term('allRawByteGrammarAndCompaction',q['per_input_upper']*a['raw_byte_units']+q['fixed_upper']*a['raw_byte_calls'],'RawBytes.(551+51)*original_byte_units +135*raw_byte_calls')
        q=p['Assert-TaskRawJson'];term('allRawStringGrammars',q['per_input_upper']*a['raw_string_units']+q['fixed_upper']*a['raw_string_calls'],'RawString.(241+113+62+40+34+36)*original_slice_units+103*raw_string_calls')
        term('allCanonicalOwn',p['Get-TaskCanonicalPath']*a['Canonical'],'max(39,51,65,76,75)*Canonical_calls; all own alias dispatches included')
        q=p['Get-TaskCatalogueEntryPath'];term('allEntryPathOwn',q['tilde']*a['EntryPath']+(q['first']-q['tilde'])*a['firstBuild_upper'],'Entry.tilde*Entry_calls+(Entry.first-Entry.tilde)*(1294+3), both tilde branch events included in both alternatives')
        term('initializeCatalogue',p['Initialize-TaskPathCatalogue'],'InitializeCatalogue.complete once, source row factors and nested Add bodies expanded')
        assert len(C)==54
        phases.append({'id':a['id'],'profile':a,'composition':C,'equations':equations,'upper':sum(C.values()),'expected_control_calls':expectedcalls,
                       'expected_witness_keys':expectedkeys,'encode_caps':caps,'encoded_preLF_units':encoded_units})
    return dict(phases=phases,upper=max(x['upper'] for x in phases),generic_FileSet_refusal={'expected_max':n,'observed_max':o,'whole':failedset,'guaranteed_replaced_fixed':sf['fixed'],'two_replacements':2*(failedset-sf['fixed'])},reference_controls_upper=refcontrols,
                replacement_helpers=replacement,controller_upper=controller,trace_claimed=False,accepted=False,source_coupled_domination_not_padding=True)
