"""New complete source-built wire shape recipes (data, not target encoding)."""
from collections import Counter

def text(units=512):return {'scalar':'string','unit_upper':units}
def integer():return {'scalar':'integer','unit_upper':20}
def boolean():return {'scalar':'boolean','unit_upper':5}
def obj(**fields):return {'map':fields}
def array(items):return {'array':items}
def descriptor(relative=False):
    d=dict(path=text(),bytes=integer(),sha256=text(64),origin_bundle_sha256=text(64))
    if relative:d['relative_path']=text()
    return obj(**d)
def mutation():return obj(path=text(),offset=integer(),before=text(4),after=text(4))
def record_control(name,witness,mutant=False,gui=False):
    d=dict(id=text(len(name)),gate=text(128),witness=witness)
    if mutant:d.update(mutation=mutation(),initial_copy_sha256=text(64))
    if gui:d['actual_mutant_sha256']=text(64)
    return obj(**d)
def child():
    # Source248: log_cap is a Boolean, not an integral alternative. Null actual
    # exits/positions/failure are dominated at their own scalar positions.
    return obj(name=text(23),started=boolean(),raw_exit_observed=boolean(),raw_exit=integer(),
      timed_out=boolean(),poll_limited=boolean(),polls=integer(),poll_limit=integer(),log_cap=boolean(),
      stdout_start=integer(),stderr_start=integer(),raw_stdout_bytes=integer(),raw_stderr_bytes=integer(),
      position_record_complete=boolean(),raw_bytes_retained=integer(),raw_bytes_observed=integer(),
      observed_not_confirmed_retained=integer(),omitted_at_cap=integer(),unknown_undrained=boolean(),both_eof=boolean(),
      reads_settled=boolean(),owned_storage_quarantined=boolean(),cleanup=array([text(43) for _ in range(13)]),failure=text(64))
def summary(controls):
    # Exact source-selected status at887 has42 units. The received parser gate
    # admits128 before equality; only the validated result enters this graph.
    return obj(status=text(42),source_manifest_sha256=text(64),entries=integer(),inverse_records=integer(),
      reserved=integer(),used=integer(),prior=integer(),composed_after=integer(),vendor=boolean(),quality=boolean(),
      Ready=boolean(),compiler_peak_disk_qualified=boolean(),actual_control_witnesses=array([
          obj(control=text(64),gate=text(128),operands=array([integer() for _ in range(6)])) for _ in range(controls)]))
def build_shapes(input_count):
    controls=array([
      record_control('json-scalar-one-past-boundary',obj(observed=integer(),maximum=integer())),
      record_control('poll-one-past-boundary',obj(completed=integer(),limit=integer(),attempted=integer())),
      record_control('work-one-below',obj(**{'total':integer(),'limit':integer(),'global':integer()})),
      record_control('bytes-one-below',obj(**{'total':integer(),'limit':integer(),'global':integer()})),
      record_control('one-use-already-consumed',obj(path=text(),exists=boolean())),
      record_control('own-tool-or-predecessor-changed',obj(path=text(),expected_bytes=integer(),observed_bytes=integer(),expected_hash=text(64),observed_hash=text(64)),True),
      record_control('pe-gui-subsystem',obj(signature=integer(),machine=integer(),magic=integer(),subsystem=integer()),True,True),
      record_control('missing-inverse-descriptor',obj(observed=integer(),expected=integer(),missing=text(42),unknown=text(0),duplicate=text(0))),
      record_control('extra-file',obj(observed=integer(),expected=integer(),missing=text(0),unknown=text(12),duplicate=text(0))),
      record_control('duplicate-file',obj(observed=integer(),expected=integer(),missing=text(0),unknown=text(0),duplicate=text()))])
    receipt=obj(schema=integer(),stage=text(11),status=text(64),source_manifest_sha256=text(64),compiler_is_opaque=boolean(),
      compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified=boolean(),old_broad_compiler_output_bound_fulfilled=boolean(),
      children=array([child() for _ in range(3)]),compiler_exit=integer(),producer_exit=integer(),verifier_exit=integer(),vendor=boolean(),
      quality=boolean(),context=boolean(),Ready=boolean(),renderer=boolean(),reserved_source_work=integer(),reserved_admitted_retained=integer(),
      runner_controls=controls,compiler_final_outputs=obj(files=array([descriptor() for _ in range(3)]),bytes=integer()),
      source_dependencies=obj(includes=integer(),report_is_additional_artifact_only=boolean(),complete_tool_inventory_replaced=boolean()),
      producer=summary(3),verification=summary(11),runner_used=integer(),failure_code=text(64),primary_failure=boolean(),
      secondary_failures=array([obj(site=text(32),code=text(43),count=integer()) for _ in range(11)]),
      failure_inventory_complete=boolean(),failure_inventory_code=text(28),compiler_final_outputs_admitted=boolean())
    marker=obj(stage=text(11),manifest=text(64),work=integer(),retained_reservation=integer(),attempts=integer(),opaque_compiler_peak_disk_qualified=boolean())
    failure=obj(files=array([obj(path=text(),bytes=integer(),reparse=boolean(),admission=text(26),opaque_temporary=boolean()) for _ in range(78)]),
      complete=boolean(),code=text(28),root_prefix=text(),compiler_temporary_peak_disk_qualified=boolean(),admitted_pool_success=boolean(),quarantine_retains_ownership=boolean())
    bundle=obj(schema=integer(),stage=text(11),status=text(64),source_manifest_sha256=text(64),
      predecessor_bundle=obj(path=text(),sha256=text(64),artifacts=integer(),transitive=integer(),complete_union_paths=integer(),complete_union_bytes=integer()),
      artifacts=array([descriptor(True) for _ in range(56)]+[descriptor() for _ in range(3+4+4)]),
      transitive_inputs=array([descriptor() for _ in range(input_count+5)]),admitted_retained_excluding_bundle=integer(),bundle_bytes=integer(),
      admitted_retained=integer(),reserved_source_work=integer(),runner_used=integer(),compiler_operation=text(6),
      compiler_temporary_peak_disk_qualified=boolean(),compiler_CPU_internal_heap_RSS_qualified=boolean(),old_broad_compiler_output_bound_fulfilled=boolean(),
      vendor=boolean(),quality=boolean(),context=boolean(),Ready=boolean(),renderer=boolean())
    return dict(marker=marker,receipt=receipt,failure=failure,bundle=bundle)

def graph_counts(shape):
    counts=Counter()
    def visit(node,depth):
        counts['max_depth']=max(counts['max_depth'],depth);counts['value_calls']+=1
        if 'map' in node:
            fields=node['map'];counts['maps']+=1;counts['keys']+=len(fields);counts['key_units']+=sum(len(k) for k in fields)
            counts['map_separators']+=max(0,len(fields)-1)
            for v in fields.values():visit(v,depth+1)
        elif 'array' in node:
            xs=node['array'];counts['arrays']+=1;counts['array_elements']+=len(xs);counts['array_separators']+=max(0,len(xs)-1)
            for v in xs:visit(v,depth+1)
        else:
            counts[node['scalar']+'s']+=1
            if node['scalar']=='string':counts['string_units']+=node['unit_upper']
    visit(shape,0);counts['nodes_keys']=counts['value_calls']+counts['keys']
    return dict(counts)
