"""Fresh complete cap-domain schedule; integer geometry, no target execution."""
def chunks(n):return (n+65535)//65536
def reconstruct_schedule(m,manifest_bytes):
    I=len(m['inputs']);own=len(m['sources']);namespaces=m['closed_directory_inputs'];ns=len(namespaces)
    guards=sum(x['entry_count']+1 for x in namespaces)
    dirs=sum(x['entry_count']+2 if x['recursive'] else 1 for x in namespaces)
    files=sum(len(x['files']) for x in namespaces)
    fence=I+own+1;fence_chunks=sum(chunks(x['bytes']) for x in m['inputs']+m['sources'])+chunks(manifest_bytes)
    rmax=max(x['bytes_cap'] for x in m['references']);bmax=max(x['bytes_cap'] for x in m['compiler_final_outputs'])
    ref_chunks=chunks(152627224+rmax)+60-1;build_chunks=chunks(67108864+bmax)+4-1
    control_chunks=sum(chunks(x['bytes_cap']) for x in m['control_files'])
    n=max(max(len(x['files']) for x in namespaces),56,67,11)
    o=max(max(x['entry_count']+1 for x in namespaces),60,78,11)
    # Ordered lifecycle prefixes, not mutually feasible maximum-action traces.
    stage=[0,7,264,523,782,784];fences=[1,1,3,5,8,8];children=[0,0,1,2,3,3]
    refs=[0,0,0,1,2,2];builds=[0,0,1,1,2,2];roots=[0,1,1,1,2,2]
    native=[0,0,0,1,2,2];copies=[0,1,2,2,2,2];markers=[0,1,1,1,1,1]
    receipts=[0,2,2,3,4,4];failures=[0,1,1,1,1,1];bpasses=[0,0,0,0,0,5]
    phases=[]
    for i in range(6):
        S,f,c,r,b,rw,np=stage[i],fences[i],children[i],refs[i],builds[i],roots[i],native[i]
        made=int(i>0);dep=int(i>=2);prod=int(i>=3);verified=int(i>=4);bundle=int(i==5)
        hash_other=3*made+4*dep+np+4*verified+5*bundle
        chunk_other=3*chunks(98009)*made+4*chunks(33554432)*dep+np*chunks(33554432)+control_chunks*verified+bundle*(4*chunks(67108864)+chunks(1048576))
        H=f*fence+60*r+4*b+hash_other;B=f*fence_chunks+r*ref_chunks+b*build_chunks+chunk_other
        W=S+f*ns+r+b+rw+1;D=S+f*dirs+61*r+b+79*rw+1
        E=6*S+(3 if S else 0)+f*guards+60*r+4*b+78*rw+1
        NL=H+f*ns+60*r+78*verified+1+D+2*copies[i]
        G=NL+2*W+E+dep
        RS=I*(f+1+bundle)+f*ns+dep*(m['profile']['include_input_count']+1)+1+made+dep
        modulecontrols=0 if np==0 else 3 if np==1 else 14
        setslots=f*ns+r+b+prod+np+verified
        setelements=f*files+56*r+3*b+56*prod+modulecontrols+67*verified
        fresh_normal=f*(fence-1)+(0 if not made else 2+dep+np)
        fresh_refusal=1 if not made else 1+(2 if i<3 else 3)
        phases.append(dict(id=f'P{i}',stage=S,fences=f,children=c,references=r,builds=b,root_walks=rw,native_parses=np,copies=copies[i],
          markers=markers[i],receipts=receipts[i],failures=failures[i],bundle_passes=bpasses[i],H=H,hash_chunks=B,noncapture_hashes=hash_other,noncapture_chunks=chunk_other,
          walkers=W,directories=D,normal_rows=E,walker_Canonical=2*W+E,NoLink=NL,Canonical=G,Resolve=RS,EntryPath=G+RS,
          firstBuild_upper=1294+3,FileSet_slots=setslots,FileSet_success_elements_upper=setelements,FileSet_generic_refusal_replacements=2,
          FileSet_generic_expected_max=n,FileSet_generic_observed_max=o,actual_module_controls=modulecontrols,
          Measure=markers[i]+receipts[i]+failures[i]+bpasses[i],Encode=markers[i]+receipts[i]+failures[i]+bundle,
          DocumentWrite=markers[i]+2*receipts[i]+2*failures[i]+2*bundle,
          raw_byte_calls=2+dep+3*made,raw_byte_units=manifest_bytes+98009+61*made+4194304*dep,
          raw_string_calls=0 if np==0 else 4 if np==1 else 16,raw_string_units=np*65536,
          Fresh_normal=fresh_normal,Fresh_refusals=fresh_refusal,hex_tests=f*fence+2*made,binary_tests=dep+np,
          wire_hex=60*r+4*b+(0 if not made else 2+2*dep+4*verified+5*bundle),outer_async_read_calls=259*c,
          cleanup_helper_attempts=14*c,cleanup_stored_rows=13*c,ref_capture_hashes=60*r,build_capture_hashes=4*b,
          postcompiler=dep,postproducer=prod,postverify=verified,bundle=bundle,created=made))
    return dict(schema='R13_R3_FRESH_CAP_DOMAIN_SCHEDULE',facts=dict(input_count=I,source_count=own,namespace_count=ns,namespace_guard_rows=guards,
      directory_upper=dirs,namespace_files=files,fence_identities=fence,fence_chunks=fence_chunks,ref_capture_max=60,build_capture_max=4,
      ref_pool=152627224,ref_largest_last=rmax,ref_chunk_upper=ref_chunks,build_pool=67108864,build_largest_last=bmax,build_chunk_upper=build_chunks,
      generic_FileSet_expected_max=n,generic_FileSet_observed_max=o,control_file_chunks=control_chunks,final_stage_individual_cap_chunks=4096,final_bundle_cap_chunks=16),
      phases=phases,trace_claimed=False,resource_fit=False,source_work_acceptance=False,execution_GO=False)
