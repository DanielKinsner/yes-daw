"""Own bounded file/text/JSON integrity checks for the boundary argument only.
No target parser, compiler, program, native API or previous checker is invoked.
"""
from pathlib import Path
import hashlib,json
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
if (HERE/'audit.json').exists():raise ValueError('Preserve first complete own green')
checks=[];inputs=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    b=p.read_bytes()
    try:s=p.relative_to(ROOT).as_posix()
    except ValueError:s=p.as_posix()
    inputs.append(dict(path=s,bytes=len(b),sha256=sha(b)))
    return b
def ck(name,condition):
    checks.append(dict(name=name,passed=bool(condition)))
    if not condition:raise ValueError(name)
contract_path=ROOT/'build-ci/20261105-clock-oracle-separate-research-contract/contract.md'
contract=read(contract_path);lines=contract.decode().splitlines()
ck('whole exact contract',sha(contract)=='344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010')
ck('122 contract lines',len(lines)==122)
for first,last,needle in [(21,24,'32MiB controlled explicit scratch'),(41,46,'Optional NRVO alternatives'),(47,50,'every controlled payload/clone/temporary'),(95,97,'quarantine in simultaneous payload phases'),(101,104,'Bound C++ exception diagnostic'),(109,110,'general CLR heap'),(112,122,'separate root one-attempt GO')]:
    ck('authority'+str(first)+'-'+str(last),needle in '\n'.join(lines[first-1:last]))
old=ROOT/'build-ci/20261106-clock-native-child-source-owners-r1'
oldpins_bytes=read(old/'source-pins.json')
ck('exact frozen owner pins',sha(oldpins_bytes)=='D665AF77E103BDFE8F2D12D81D04F9E90C3C95A430343E673AC0D8F3597C8E4A')
oldpins=json.loads(oldpins_bytes)
for row in oldpins['files']:
    p=ROOT/row['path'];b=read(p)
    ck('frozen owner '+p.name,len(b)==row['bytes'] and sha(b)==row['sha256'])
for row in oldpins['inputs']:
    p=Path(row['path']) if ':' in row['path'] else ROOT/row['path'];b=read(p)
    ck('frozen input '+p.name,len(b)==row['bytes'] and sha(b)==row['sha256'])
include=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include')
exc=read(include/'vcruntime_exception.h').decode();stdexcept=read(include/'stdexcept').decode()
ck('exception copy request', '__std_exception_data _InitData = { _Message, true };' in exc and '__std_exception_copy(&_InitData, &_Data);' in exc)
ck('logic_error delegates source message','explicit logic_error(const char* _Message) : _Mybase(_Message) {}' in stdexcept)
ck('runtime_error delegates source message','explicit runtime_error(const char* _Message) : _Mybase(_Message) {}' in stdexcept)
article=read(HERE/'audit.md').decode()
leaves=['ABI_FRAME','CRT_FILE_NATIVE','CRT_STARTUP_TLS','EH_ALLOCATOR','DATA_AND_PROFILE','JOINT_CONTROLLER']
for leaf in leaves:ck('prior leaf covered '+leaf,leaf in article)
for phrase in ['Recommendation A','Alternative B','sizeof','32768','controlled source-object upper','not expressly','No probe or qualification source was authored or run','No price or Data','all original ownersR1']:
    ck('disclosure '+phrase,phrase.lower() in article.lower())
classifications=[
 dict(id='ABI_FRAME',A='All real own/selected-STL objects, object padding, parameters/temps/no-NRVO alternatives',B='No blanket backend-stack exclusion asserted',C='Complete source lifetime and selected layout gates',uncertain='Backend-only spill/shadow/frame reservation is not certified; no physical-stack claim; narrow metric interpretation awaits independent review'),
 dict(id='CRT_FILE_NATIVE',A='Stream/source-requested buffers and controlled path/output clones including failures',B='Generic primitive FILE/fd/handle/runtime bookkeeping only',C='Selected behavior/decomposition for claimed absence or controlled buffers',uncertain='Opaque aggregate FILE allocation cannot be entirely discounted if controlled buffer coexists'),
 dict(id='CRT_STARTUP_TLS',A='Explicit source launcher/child string or buffer owners and identified controlled clones',B='General CRT startup/TLS/loader/runtime machinery',C='Exact input/profile/association facts actually used',uncertain='Surfaced argv/env referent clone versus generic startup storage is not itemized expressly; no exclusion-dependent binding'),
 dict(id='EH_ALLOCATOR',A='Actual source thrown objects, real copies/witnesses and bounded source-message copy referents',B='Generic EH/TLS/unwinder/allocator headers,rounding,retention/RSS',C='Selected message-copy capacity/multiplicity and source/layout association',uncertain='Existing CLR exception exclusion does not itself remove C++ source exception/message objects'),
 dict(id='DATA_AND_PROFILE',A='Actual bound Catalogue/data owner if later defined',B='No new opaque discount',C='Selected compiler/header/source/runtime behavior identities plus actual binding',uncertain='No definition or true guards exist'),
 dict(id='JOINT_CONTROLLER',A='Actual simultaneous controlled controller/child/quarantine payload',B='General runtime/RSS remains unqualified, not zero',C='Complete phase and observed settlement association',uncertain='No per-process split/reset or unobserved exit credit')]
ck('exact six prior leaves',[r['id'] for r in classifications]==leaves)
result=dict(schema='CLOCK_NATIVE_RESOURCE_EXISTING_BOUNDARY_AUDIT_R1',verdict='APPLY_EXISTING_CONTROLLED_SOURCE_BOUNDARY_WITH_NARROW_UNCERTAIN_SUBCASES_HOLD',recommendation='A: complete controlled source objects/buffers and identity/phase gates; disclose general-runtime limits; do not demand or claim whole-process RSS/CRT proof',authority=dict(path=contract_path.relative_to(ROOT).as_posix(),sha256=sha(contract),excerpts=[dict(lines=[a,b],text='\n'.join(lines[a-1:b])) for a,b in [(21,29),(35,53),(95,110),(112,122)]]),classifications=classifications,automatic_object_answer='A controlled source-object upper can be derived without a physical stack/RSS certificate, conditional on complete objects/temporaries/no-NRVO lifetimes and selected layout correspondence; current2S/4T and unexplained32768 alone are insufficient.',compile_only_route='Future separately authored/reviewed/pinned minimal type/layout purpose TU, sizeof/alignof static_asserts only; root GO required before compiler. No link/target main/Catalogue or run. Replica correspondence gate required. Not authored here.',prior_frozen_owner_packet_unchanged=True,new_aggregate_upper_fit_Data_flags_or_authority=False,no_target_or_qualification_execution=True,checks=checks,input_pins=inputs)
(HERE/'audit.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='OWN_BOUNDARY_SOURCE_CHECK_EXIT0',checks=len(checks),verdict=result['verdict'],audit_json_sha256=sha((HERE/'audit.json').read_bytes()))))
