"""Source-sized R13 phase/cap-domain schedule, independent of scalar prices.

All counts are upper schedule components, not a feasible all-outcomes trace.
No filesystem snapshot-uniqueness or immutable-owned-output premise is used.
"""

def ceil_chunks(n): return (n+65535)//65536

def derive(manifest,manifest_bytes):
    I=len(manifest['inputs']);own=len(manifest['sources']);NS=len(manifest['closed_directory_inputs'])
    EG=sum(x['entry_count']+1 for x in manifest['closed_directory_inputs'])
    DG=sum(x['entry_count']+2 if x['recursive'] else 1 for x in manifest['closed_directory_inputs'])
    files=sum(len(x['files']) for x in manifest['closed_directory_inputs'])
    fence_identities=I+own+1
    fence_chunks=sum(ceil_chunks(x['bytes']) for x in manifest['inputs']+manifest['sources'])+ceil_chunks(manifest_bytes)
    ref_pool=152627224;ref_largest=max(x['bytes_cap'] for x in manifest['references'])
    build_pool=67108864;build_largest=max(x['bytes_cap'] for x in manifest['compiler_final_outputs'])
    ref_capture_max=60;build_capture_max=4
    ref_chunks=ceil_chunks(ref_pool+ref_largest)+ref_capture_max-1
    build_chunks=ceil_chunks(build_pool+build_largest)+build_capture_max-1
    control_chunks=sum(ceil_chunks(x['bytes_cap']) for x in manifest['control_files'])
    fs_expected_max=max(max(len(x['files']) for x in manifest['closed_directory_inputs']),56,67,11)
    fs_observed_max=max(max(x['entry_count']+1 for x in manifest['closed_directory_inputs']),60,78,11)
    specs=dict(fences=[1,1,3,5,8,8],children=[0,0,1,2,3,3],stage=[0,7,264,523,782,784],
               references=[0,0,0,1,2,2],builds=[0,0,1,1,2,2],root_walks=[0,1,1,1,2,2],
               native_parses=[0,0,0,1,2,2],copies=[0,1,2,2,2,2],
               markers=[0,1,1,1,1,1],receipts=[0,2,2,3,4,4],failures=[0,1,1,1,1,1],
               bundle_passes=[0,0,0,0,0,5])
    phases=[]
    for i in range(6):
        a={k:v[i] for k,v in specs.items()}
        f=a['fences'];c=a['children'];S=a['stage'];r=a['references'];b=a['builds'];rw=a['root_walks'];native=a['native_parses']
        created=int(i>0);postcompiler=int(i>=2);postproducer=int(i>=3);postverify=int(i>=4);bundle=int(i==5)
        # Main and wrapper hashing sites:3predecessor/copy/control;4binary/GUI;
        # one post-native captured-binary check each;4controlfiles;4stage+bundle.
        noncapture_hashes=3*created+4*postcompiler+native+4*postverify+5*bundle
        noncapture_chunks=3*ceil_chunks(98009)*created+4*ceil_chunks(33554432)*postcompiler+native*ceil_chunks(33554432)+control_chunks*postverify
        noncapture_chunks+=bundle*(4*ceil_chunks(67108864)+ceil_chunks(1048576))
        H=f*fence_identities+r*ref_capture_max+b*build_capture_max+noncapture_hashes
        B=f*fence_chunks+r*ref_chunks+b*build_chunks+noncapture_chunks
        W=S+f*NS+r+b+rw+1
        D=S+f*DG+61*r+b+79*rw+1
        E=6*S+(3 if S else 0)+f*EG+60*r+4*b+78*rw+1
        CW=2*W+E
        # Per-file extra NoLink call in References, plus main root/run directory
        # checks and all walker directories; two explicit Copy path checks each.
        NL=H+f*NS+ref_capture_max*r+78*postverify+1+D+2*a['copies']
        G=NL+CW+postcompiler
        resolve_other=1+created+postcompiler
        RS=I*(f+1+bundle)+f*NS+postcompiler*(manifest['profile']['include_input_count']+1)+resolve_other
        EP=G+RS
        controls=0 if not native else (3 if native==1 else 14)
        set_slots=f*NS+r+b+postproducer+native+postverify
        success_set_elements=f*files+56*r+3*b+56*postproducer+controls+67*postverify
        measures=a['markers']+a['receipts']+a['failures']+a['bundle_passes']
        encodes=a['markers']+a['receipts']+a['failures']+bundle
        docwrites=a['markers']+2*a['receipts']+2*a['failures']+2*bundle
        # Generic Fresh failures: before root, at most the initial pre-fence;
        # compiler wrapper can give pre+finally; native additionally binary.
        # Earlier expected predecessor control adds exactlyone after creation.
        fresh_refusals=(1 if i==0 else (1+(2 if i<3 else 3)))
        fresh_normal=f*(fence_identities-1)+(0 if not created else (2+postcompiler+native))
        hex_tests=f*fence_identities+2*created
        binary_tests=postcompiler+native
        wire_main=0 if not created else (2+2*postcompiler+4*postverify+5*bundle)
        wire_hex=ref_capture_max*r+build_capture_max*b+wire_main
        phases.append(dict(id='P'+str(i),**a,H=H,hash_chunks=B,noncapture_hashes=noncapture_hashes,noncapture_chunks=noncapture_chunks,
                           walkers=W,directories=D,normal_rows=E,walker_Canonical=CW,NoLink=NL,Canonical=G,Resolve=RS,EntryPath=EP,
                           firstBuild_upper=1294+3,FileSet_slots=set_slots,FileSet_success_elements_upper=success_set_elements,
                           FileSet_generic_refusal_replacements=2,FileSet_generic_expected_max=fs_expected_max,FileSet_generic_observed_max=fs_observed_max,
                           actual_module_controls=controls,Measure=measures,Encode=encodes,DocumentWrite=docwrites,
                           raw_byte_calls=2+postcompiler+3*created,
                           raw_byte_units=manifest_bytes+98009+61*created+4194304*postcompiler,
                           raw_string_calls=0 if not native else (4 if native==1 else 16),raw_string_units=native*65536,
                           Fresh_normal=fresh_normal,Fresh_refusals=fresh_refusals,hex_tests=hex_tests,binary_tests=binary_tests,wire_hex=wire_hex,
                           outer_async_read_calls=259*c,cleanup_helper_attempts=14*c,cleanup_stored_rows=13*c,
                           ref_capture_hashes=ref_capture_max*r,build_capture_hashes=build_capture_max*b,
                           postcompiler=postcompiler,postproducer=postproducer,postverify=postverify,bundle=bundle,created=created))
    facts=dict(input_count=I,source_count=own,namespace_count=NS,namespace_guard_rows=EG,directory_upper=DG,namespace_files=files,
               fence_identities=fence_identities,fence_chunks=fence_chunks,ref_capture_max=ref_capture_max,build_capture_max=build_capture_max,
               ref_pool=ref_pool,ref_largest_last=ref_largest,ref_chunk_upper=ref_chunks,
               build_pool=build_pool,build_largest_last=build_largest,build_chunk_upper=build_chunks,
               generic_FileSet_expected_max=fs_expected_max,generic_FileSet_observed_max=fs_observed_max,control_file_chunks=control_chunks,
               final_stage_individual_cap_chunks=4*ceil_chunks(67108864),final_bundle_cap_chunks=ceil_chunks(1048576))
    proofs=[
      'Hash row pool guard is AFTER GetHash. Before the final capture sum<=pool; final reached capture<=largest cap. sumLength<=pool+largestLast; sum ceil(length/65536)<=ceil(sumLength/65536)+N-1. No refund after later failure.',
      'Reference/build capture60/4 is distinct from exact successful return56/3. Closed consumer graphs and later main loops only see successful returned sets; no snapshot-unique enumeration premise.',
      'Final stage hashes each receive64MiB cap AFTER last stage admission;4individual1024chunk terms are retained before later aggregate/stage/retained refusal. No nominal3MiB substitution.',
      'Fence manifest may change before manifest hash check; it can use16chunks thenabort before own/input hashes. Unreached own/input normal fence chunks exceed its8extra chunks, so full fence7862 still dominates explicitly within that same fence.',
      'Each GetEntries has at mostMaximum captured rows plusonefirst-overflow candidate; stage normal captures<=6S+3 over three first-failing main/inventory/footer scans. Overflow own prefix is separately priced, without a fake normal ending.',
      'Generic FileSet failures replace at mosttwo declared slots (e.g. preFence and mandatoryfinally). Context maxima derive from actual namespaces/root/ref/build/child call sites, not arbitrary arrayprofile1225. A replacement may subtract only the guaranteed minimum normal fixed cost, not an uncharged maximum-N normal price.',
      'Fresh generic failures can include native prePins+finallyPins+binary in one finally, plus earlier expected control=4. No immutable-files or quiet-output assumption.',
      '13 stored cleanup rows do not imply13 helper attempts: first reap diagnostic may throw inside try and its enclosing catch makes exactlyone further attempt. No retry beyond written sites; later helper failure terminates to outer footer catch.',
      '259 outer reads follows2initial+257positive requeues; a1MiB cap can retain255full4096 and twoEOF-short positives, then terminalzeros. Underlying ReadAtLeast inner calls are not bounded by259 and stay V-resource terms/opaque CPU.'
    ]
    return dict(schema='R13_R2_SOURCE_DERIVED_PHASE_CAP_SCHEDULE_V1',facts=facts,phases=phases,proofs=proofs,
                claimed_execution_trace=False,resource_fit=False,selected_limit_or_Data_or_GO=False,
                source_functionality='HOLD_CONFIRMED_TYPE_ARRAY_EMPTYTYPES_DEFECT')
