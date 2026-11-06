"""Conditional environment storage, never reads the host environment."""
from pathlib import Path
from hashlib import sha256
import json,ntpath,re,struct
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
manifest=read(base/'source-manifest.json')
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def incoming_hashtable(E):
 c=3;cs=[3]
 for inserted in range(E):
  if inserted>=int(f32(f32(.72)*c)):c=prime(2*c);cs.append(c)
 return dict(capacities=cs,requests=72+sum(arr(c,24) for c in cs))
E=15;Vmax=33456;Bnative=66974
H=incoming_hashtable(E)
dc=prime(E)
incoming=dict(native_GetEnvironmentStringsW_blocks= Bnative,
 key_and_value_strings=2*Vmax+56*E,
 inherited_Hashtable=H['requests'],inherited_Dictionary=80+arr(dc,4)+arr(dc,24),
 manual_Hashtable_enumerator=72,cached_Dictionary_key_view=24)
assert H['capacities']==[3,7,17,37] and dc==17
assert sum(map(len,manifest['environment']))==43
fixed=sum(len(k)+len(v) for k,v in manifest['environment'].items() if k not in ['TEMP','TMP'])
assert fixed==569
run=ntpath.join(str(base),'runs','source-clock-r1');opaque=ntpath.join(run,'opaque-temp')
L=fixed+len('TEMP')+len('TMP')+2*len(opaque)+2*8
assert L==968
def outgoing(L):
 # Append spans<=512. All old chunks are full at ExpandByABlock, and total
 # length<8000, so each new capacity is at least the previous total. The
 # minimum-growth sequence starts64 and doubles total; this bounds count.
 c=64;chunks=1
 while c<L:c*=2;chunks+=1
 maxnew=max(512,L-1)
 capacity=L+maxnew-1
 terms=dict(StringBuilder_all_old_arrays=2*capacity+30*chunks,
  StringBuilder_all_old_objects=56*chunks,outgoing_environment_string=a8(22+2*L),sorted_keys_string_array=arr(8,8))
 return dict(output_units=L,chunk_count_upper=chunks,all_chunk_capacity_upper=capacity,terms=terms,requests=sum(terms.values()))
O=outgoing(L)
incoming_total=sum(incoming.values())
per=incoming_total+O['requests']
out=dict(schema='R12_ENVIRONMENT_CONDITIONAL_SOURCE_COMPONENT_R1',status='CONDITIONAL_ORDINARY_STOCK_PROFILE_NOT_ACTUAL_ENVIRONMENT_QUALIFICATION',
 no_host_environment_read=True,environment_values_written_or_dumped=False,
 source_profile=dict(entries=E,key_value_units_equation='690+PSModulePathUnits',PSModulePath_units_upper=32766,
  key_value_units_upper=Vmax,native_bytes_upper=Bnative,valid_unique_nonhidden_block_required=True,
  ordinary_selected_SDK_noninterference_during_all_three_clone_intervals_required=True),
 incoming_terms=incoming,incoming_Hashtable_capacities=H['capacities'],incoming_Dictionary_capacity=dc,
 outgoing_profile=dict(public_manifest_fixed_key_value_units_excluding_TEMP_TMP=fixed,opaque_path_units=len(opaque),**O),
 per_child_numeric_selected=per,per_child_still_nonzero='DictionaryWrapper instance field-slot upper v_DW, pending selected source association',
 phases=[dict(id='P'+str(i),children=c,numeric_selected=per*c,whole_environment_equation=f'{per*c} + {c}*v_DW') for i,c in enumerate([0,0,1,2,3,3])],
 safety_model=['No second key/value string copy when Dictionary inherits Hashtable values; references are shared.',
  'Manual IDictionaryEnumerator.Entry avoids per-entry boxed DictionaryEntry allocations in this clone path.',
  'Clear retains inherited Dictionary arrays;8 outgoing additions fit17 and cannot trigger>100 collision rehash.',
  'Native GetEnvironmentStringsW copy is retained conservatively for every clone: selected Release source only Debug.Assert checks FreeEnvironmentStringsW success. No successful-free credit is needed.',
  'Outgoing native CreateProcess pins the produced String; do not fabricate another native block copy. OS child environment storage remains separate native/process qualification.',
  'Ordinary selected SDK/provider/build/stock environment facts are prerequisites, not measured or unconditionally true. No sampled environment, sleep, fresh process or absence of hooks proves this profile by itself.'])
(HERE/'environment.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(incoming=incoming,incoming_total=incoming_total,outgoing=O,per_child_numeric_selected=per),indent=2))
