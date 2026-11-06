"""Source-built output schema expansion, no target code or old census import.

Each scalar is a worst alternative at its actual schema position. Strings in
receipt/failure are conservatively admitted512, not an unproved exact run-root
profile. Null alternatives are bounded by the larger integer/string branch.
"""
from collections import Counter

def S(n=512): return ('string',n)
I=('integer',0)
B=('boolean',0)
def M(**d): return d
def A(items): return list(items)
def descriptor(relative=False):
    d=M(path=S(),bytes=I,sha256=S(64),origin_bundle_sha256=S(64))
    if relative:d['relative_path']=S()
    return d
def mutation():return M(path=S(),offset=I,before=S(4),after=S(4))
def control(name,witness,mutated=False,gui=False):
    d=M(id=S(len(name)),gate=S(128),witness=witness)
    if mutated:d.update(mutation=mutation(),initial_copy_sha256=S(64))
    if gui:d['actual_mutant_sha256']=S(64)
    return d
def child():
    # Exact24 fields at source248; integer alternatives cover raw null/number.
    return M(name=S(23),started=B,raw_exit_observed=B,raw_exit=I,
      timed_out=B,poll_limited=B,polls=I,poll_limit=I,log_cap=I,
      stdout_start=I,stderr_start=I,raw_stdout_bytes=I,raw_stderr_bytes=I,
      position_record_complete=B,raw_bytes_retained=I,raw_bytes_observed=I,
      observed_not_confirmed_retained=I,omitted_at_cap=I,unknown_undrained=B,
      both_eof=B,reads_settled=B,owned_storage_quarantined=B,
      cleanup=A([S(43) for _ in range(13)]),failure=S(64))
def summary(n):
    # Only12 validated fields and source-built control records survive889..892.
    return M(status=S(128),source_manifest_sha256=S(64),entries=I,
      inverse_records=I,reserved=I,used=I,prior=I,composed_after=I,
      vendor=B,quality=B,Ready=B,compiler_peak_disk_qualified=B,
      actual_control_witnesses=A([M(control=S(64),gate=S(128),operands=A([I]*6)) for _ in range(n)]))
def build():return M(files=A([descriptor() for _ in range(3)]),bytes=I)
def runner_controls():
    return A([
      control('json-scalar-one-past-boundary',M(observed=I,maximum=I)),
      control('poll-one-past-boundary',M(completed=I,limit=I,attempted=I)),
      control('work-one-below',M(total=I,limit=I,global_=I)),
      control('bytes-one-below',M(total=I,limit=I,global_=I)),
      control('one-use-already-consumed',M(path=S(),exists=B)),
      control('own-tool-or-predecessor-changed',M(path=S(),expected_bytes=I,observed_bytes=I,expected_hash=S(64),observed_hash=S(64)),True),
      control('pe-gui-subsystem',M(signature=I,machine=I,magic=I,subsystem=I),True,True),
      control('missing-inverse-descriptor',M(observed=I,expected=I,missing=S(41),unknown=S(0),duplicate=S(0))),
      control('extra-file',M(observed=I,expected=I,missing=S(0),unknown=S(12),duplicate=S(0))),
      control('duplicate-file',M(observed=I,expected=I,missing=S(0),unknown=S(0),duplicate=S()))])
def shapes(inputs):
    # Python global_ spelling corrected to the actual wire key below.
    rc=runner_controls()
    for c in rc:
        if 'global_' in c['witness']:c['witness']['global']=c['witness'].pop('global_')
    receipt=M(schema=I,stage=S(11),status=S(64),source_manifest_sha256=S(64),
      compiler_is_opaque=B,compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified=B,
      old_broad_compiler_output_bound_fulfilled=B,children=A([child() for _ in range(3)]),
      compiler_exit=I,producer_exit=I,verifier_exit=I,vendor=B,quality=B,context=B,Ready=B,renderer=B,
      reserved_source_work=I,reserved_admitted_retained=I,runner_controls=rc,
      compiler_final_outputs=build(),source_dependencies=M(includes=I,report_is_additional_artifact_only=B,complete_tool_inventory_replaced=B),
      producer=summary(3),verification=summary(11),runner_used=I,failure_code=S(64),
      primary_failure=B,secondary_failures=A([M(site=S(32),code=S(41),count=I) for _ in range(11)]),
      failure_inventory_complete=B,failure_inventory_code=S(28),compiler_final_outputs_admitted=B)
    marker=M(stage=S(11),manifest=S(64),work=I,retained_reservation=I,attempts=I,opaque_compiler_peak_disk_qualified=B)
    failure=M(files=A([M(path=S(),bytes=I,reparse=B,admission=S(26),opaque_temporary=B) for _ in range(78)]),
      complete=B,code=S(28),root_prefix=S(),compiler_temporary_peak_disk_qualified=B,
      admitted_pool_success=B,quarantine_retains_ownership=B)
    artifacts=[descriptor(True) for _ in range(56)]+[descriptor() for _ in range(3+4+4)]
    bundle=M(schema=I,stage=S(11),status=S(64),source_manifest_sha256=S(64),
      predecessor_bundle=M(path=S(),sha256=S(64),artifacts=I,transitive=I,complete_union_paths=I,complete_union_bytes=I),artifacts=A(artifacts),
      transitive_inputs=A([descriptor() for _ in range(inputs+5)]),
      admitted_retained_excluding_bundle=I,bundle_bytes=I,admitted_retained=I,
      reserved_source_work=I,runner_used=I,compiler_operation=S(6),
      compiler_temporary_peak_disk_qualified=B,compiler_CPU_internal_heap_RSS_qualified=B,
      old_broad_compiler_output_bound_fulfilled=B,vendor=B,quality=B,context=B,Ready=B,renderer=B)
    return dict(marker=marker,receipt=receipt,failure=failure,bundle=bundle)

def count(shape):
    c=Counter()
    def walk(v,d=0):
        c['max_depth']=max(c['max_depth'],d)
        c['value_calls']+=1
        if isinstance(v,dict):
            c['maps']+=1;c['keys']+=len(v);c['key_units']+=sum(map(len,v));c['map_separators']+=max(0,len(v)-1)
            for x in v.values():walk(x,d+1)
        elif isinstance(v,list):
            c['arrays']+=1;c['array_elements']+=len(v);c['array_separators']+=max(0,len(v)-1)
            for x in v:walk(x,d+1)
        else:
            c[v[0]+'s']+=1
            if v[0]=='string':c['string_units']+=v[1]
    walk(shape)
    # Visits count keys in addition to values. Keys are literal ASCII identifiers.
    c['nodes_keys']=c['keys']+c['value_calls']
    return dict(c)

def derive(inputs):return {k:count(v) for k,v in shapes(inputs).items()}
