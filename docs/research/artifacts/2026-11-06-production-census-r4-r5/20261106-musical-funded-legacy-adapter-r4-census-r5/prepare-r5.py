"""Own ordinary file/JSON authoring only;never executes/imports frozen helpers.

The full previous source-ledger DATA remains unaccepted. This creates a whole
new proposal with explicit source corrections,not a price patch or acceptance
of the411 unlisted rows. The new generator reads JSON operands only.
"""
from pathlib import Path
from hashlib import sha256
import json,re

ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
OLD=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r4'
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def save(name,value):(HERE/name).write_bytes((json.dumps(value,indent=2)+'\n').encode())
assert digest(OLD/'census.json')=='BDBA8D8E2874BF7BD404108B575719EB0BF956C987E217F5A3DAD9E9BBE41E6E'
previous=json.loads((OLD/'census.json').read_bytes())
critic_path=ROOT/'build-ci/20261106-musical-funded-legacy-adapter-r4-numeric-independent-review-r4/review.json'
assert digest(critic_path)=='EE53162D1CFB45ECA97DA514F8F5394E94A16E01902F40D51AD4FF87508C33DC'
critic=json.loads(critic_path.read_bytes())
rows={e['site']:dict(body_vector=e['body_vector'].copy(),operations=e['source_operand_derivation'],
 helper_coefficients=e['helper_coefficients'],bare_own_identifiers=e['bare_own_identifier_or_initializer_boundary'],
 body_B_vector=e['body_B_vector'],body_d_vector=e['body_d_vector'].copy(),sponsors=e['sponsor_boundary']) for e in previous['events']}
corrections=[]
for name,component,constant,dc in [
 ('ContextDecision',2,1,0),('LegacyClockEligibility',3,1,-1),('QueryQuote',3,1,-1),
 ('QueryPredictionEndpoints',3,1,-1),('ClipRow',0,1,0),('ClipFound',0,1,0),
 ('AssetRow',0,1,0),('AssetFound',0,1,0),('MapRow',0,1,0)]:
    before=rows[name]['body_vector'].copy();before_d=rows[name]['body_d_vector'].copy()
    rows[name]['body_vector'][component]+=constant
    rows[name]['body_d_vector'][component]+=dc
    c=critic['source_findings'][name]
    assert rows[name]['body_vector']==c['necessary_body_retaining_other_proposed_components']
    assert rows[name]['body_d_vector']==c['necessary_body_d_retaining_other_proposed_components']
    corrections.append(dict(site=name,component=component,old_body=before,new_body=rows[name]['body_vector'],
      old_d=before_d,new_d=rows[name]['body_d_vector'],source=c['basis'],scope='Necessary source correction;whole new proposal still requires review.'))
for name in ('ClipRow','ClipFound','AssetRow','AssetFound','MapRow'):
    rows[name]['operations']+=' R5 whole-source SDK audit:vector[] owns one logical stored-base R in addition to the actual index transfer/field/access,as709B requires. No element referent copy or SDK forwarding fee.'
rows['ContextDecision']['operations']='checkForReceiverOnce own HZ,*state_ prefix and outside .checkForReceiverOnce caller;two cancel/inspect predicates plus own ordinary Reader-role conditional C1,for C3 total. L owns neither this conditional nor caller selector. Reader itself has no fabricated scalar R.'
rows['LegacyClockEligibility']['operations']='c.rows field/read,512+2*bit_width result arithmetic,eligibility destination and effectiveWork guard. Selected MSVC <bit>:130 returns int;outer int-to-U64 is constant X1,inner U64-to-size_t is conditional d. Math1 owns input parameter W/HZ only. Required constant/qualification/bare identifiers add no invented reads/selectors.'
rows['QueryQuote']['operations']='Rows parameter,bits/prefix/append/amount locals,twelve bool destinations;two dynamic row==0 selections,two ternary arithmetic,2*bits,eleven eager return&;nine b dots,three OldCatalogue field dots,five NativeBinding field dots. Selected bit_width returns int:outer int-to-U64 X1,inner U64-to-size_t d;Bounds/Math1 separate. Qualifiers alone X0.'
rows['QueryPredictionEndpoints']['operations']='at/prefix/threshold/eligible/rate five scalar destinations,two Endpoint13zero;rows span/size/bit_width threshold arithmetic,limits eligibility,A returned/status guards,metadata projectRate path and real A callers. span.size already returns size_t;selected bit_width returns int,so outer int-to-U64 X1 independent of d. Distinct span/limits/accessor helpers retain their own source cells/HZ.'
assert len(rows)==420 and len(previous['helpers'])==51
save('body-ledgers.json',dict(schema='UNACCEPTED_EXPLICIT_ALL420_SOURCE_OPERANDS_R5',predecessor_sha256=digest(OLD/'census.json'),
 prior_whole_verdict='REVISE_COMPLETE_SOURCE_CENSUS',no_unlisted411_acceptance=True,rows=rows))
save('helper-ledgers.json',dict(schema='UNACCEPTED_EXPLICIT_ALL51_HELPER_OPERANDS_R5',
 predecessor_sha256=digest(OLD/'helper-ledgers.py'),helpers=previous['helpers'],scope='All51 source cases re-expanded/reviewable;no positive acceptance transferred. No executable helper is imported.'))
save('corrections.json',dict(whole_critic_sha256=digest(critic_path),source_changes=False,caps_or_contract_changes=False,
 all420_all51_remain_proposals=True,corrections=corrections,
 next_whole_review_required=True))

source_root=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-source-r4'
formation=(source_root/'formation-r8.h').read_text()
methods=[('ContextScope (MusicalPreparationRequestState*','ReceiveWorker/Reader',4),
 ('~ContextScope()','ContextReturn',3),('bool cancellationRequestedOnce()','ContextCancel',1),
 ('bool inspectCapturedRequestOnce()','ContextInspect',1),('void checkForReceiverOnce()','ContextDecision',1)]
reader=[]
for i,(signature,sponsor,n) in enumerate(methods):
    start=formation.index(signature)
    end=formation.index(methods[i+1][0],start) if i+1<len(methods) else formation.index('\n};',start)
    occurrences=[]
    for m in re.finditer(r'\bReader\s*\?',formation[start:end]):
        absolute=start+m.start();line=formation.count('\n',0,absolute)+1
        occurrences.append(dict(line=line,text=formation.splitlines()[line-1].strip()))
    assert len(occurrences)==n,(sponsor,len(occurrences))
    reader.append(dict(sponsor=sponsor,reached_ordinary_conditional_count=n,Reader_scalar_R=0,
       source_occurrences=occurrences,scope='Body C includes these conditional selections,not helperL/I or a caller double charge.'))
assert sum(r['reached_ordinary_conditional_count'] for r in reader)==10
vector_uses=[]
for site,fragment in [('ClipRow','in.project_.clips[static_cast<std::size_t> (ci)].id'),
 ('ClipFound','&in.project_.clips[static_cast<std::size_t> (ci)]'),
 ('AssetRow','in.project_.assets[static_cast<std::size_t> (ai)].id'),
 ('AssetFound','&in.project_.assets[static_cast<std::size_t> (ai)]'),
 ('MapRow','in.project_.tempoMap[static_cast<std::size_t> (mi)]')]:
    assert formation.count(fragment)==1,(site,fragment)
    pos=formation.index(fragment)
    vector_uses.append(dict(site=site,line=formation.count('\n',0,pos)+1,expression=fragment,
      visible_SDK_boundary=dict(stored_base_R=1,index_parameter_W=1,subscript_X=1,H=1,Z=1),
      user_field_or_address='Actual explicit field/address/index and any scalar element use remain in the body or named ID helper. No referent copy or recursive SDK field walk.'))
assert len(re.findall(r'in\.project_\.(?:clips|assets|tempoMap)\[',formation))==5
sdk=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include')
assert digest(sdk/'bit')=='F08C873229E5C11B6E89A0D47C5CA5E2555947321AA382BF52224D041648D483'
assert digest(sdk/'vector')=='351604D91A2CEAC79F9BD9448C0553CCCC22711E22955B5938EAF41DE98334A6'
assert 'constexpr int bit_width(const _Ty _Val)' in (sdk/'bit').read_text()
math_header=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt/corecrt_math.h')
math_text=math_header.read_text()
assert 'inline bool isfinite(' in math_text
assert re.search(r'\bdouble\s+__cdecl\s+floor\(_In_ double _X\)',math_text)
assert re.search(r'\bdouble\s+__cdecl\s+log1p\(_In_ double _X\)',math_text)
bit_uses=[]
for file in ('legacy-adapter.h','window-query.h'):
    for line,text in enumerate((source_root/file).read_text().splitlines(),1):
        if 'std::bit_width' in text:bit_uses.append(dict(file=file,line=line,text=text.strip()))
assert len(bit_uses)==3
save('source-boundaries.json',dict(schema='WHOLE_R5_READER_VECTOR_SCALAR_SDK_SOURCE_BOUNDARY_INVENTORY',
 Reader_conditionals=reader,capture_vector_indices=vector_uses,
 scalar_SDK_leaf_cases=[
 dict(leaf='std::bit_width(size_t)',parameter='one selected unsigned size_t scalar destination W',result='int from pinned bit:130',
   callers=bit_uses,result_conversion='Ordinary int-to-U64 is fixed X1 at all three callers. Only first two have U64-to-size_t input d;span.size already size_t at the third.',
   inherited_CPU='Math1 remains W1/H1/Z1;selected implementation CPU is opaque,not zero or a new tariff.'),
 dict(leaf='std::isfinite(double)',parameter='double scalar W1;caller read/field/expression separate',result='bool returned predicate',source='All new classifier/replay uses have explicitly double operands;C predicate belongs reached caller.',scope='No recursive classification implementation or result-copy fee.'),
 dict(leaf='std::floor(double)',parameter='double scalar W1',result='double;explicit int64 caller conversion is X1',source='QueryEndpointOk retains frame field/read and floor destination;Math1 W/H/Z separate.'),
 dict(leaf='::log1p(double)',parameter='double scalar W1',result='double',source='Actual old append is protected190 work. Native taking of double(__cdecl*)(double) address is not invocation.',scope='No new libm CPU or equivalence qualification.')],
 fixed_array_and_span_distinction='std::array[] has no stored-base R;builtin arrays have no SDK H/Z. ReturnSpanValue owns base+extent2R/2W and selected conversion H/Z;no vector copy.',
 input_pins=[dict(path=p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),bytes=p.stat().st_size,sha256=digest(p)) for p in [source_root/'formation-r8.h',source_root/'legacy-adapter.h',source_root/'window-query.h',sdk/'bit',sdk/'vector',sdk/'cmath',math_header]],
 not_source_price_acceptance=True,no_new_source_or_cap_or_contract=True))

# Reuse mathematical source text as lineage,not execution or accepted prices.
# Own new IO/provenance and complete source corrections are authored below.
code=(OLD/'build-census.py').read_text()
start=code.index('def module(name):');end=code.index('def plus(',start)
code=code[:start]+code[end:]
code=code.replace('import importlib.util,json,re,sys','import json,re,sys')
code=code.replace("def pin(p):return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=digest(p))",
 "def pin(p):return dict(path=p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),bytes=p.stat().st_size,sha256=digest(p))")
code=code.replace("body=module('body-ledgers');helpers=module('helper-ledgers').H\nassert body.CLASS_STATIC_ID_X==0",
 "body_rows=json.loads((HERE/'body-ledgers.json').read_bytes())['rows']\nhelpers=json.loads((HERE/'helper-ledgers.json').read_bytes())['helpers']")
code=code.replace('body.ROWS','body_rows')
code=code.replace("build-ci/20261105-musical-funded-legacy-adapter-r4-census-r3/census.json","build-ci/20261105-musical-funded-legacy-adapter-r4-census-r4/census.json")
code=code.replace("YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_FRESH_CENSUS_R4_V1","YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_WHOLE_CENSUS_R5_V1")
code=code.replace("FRESH_PRODUCTION_R4_COMPLETE_CENSUS_R4_PINS","PRODUCTION_R4_COMPLETE_CENSUS_R5_PINS")
code=code.replace("# Fresh production census R4 proposal","# Complete production census R5 proposal")
code=code.replace("Imports ONLY the two newly authored sibling operand-data modules. Old packets\nare comparison inputs,never a fallback source for any new body/helper price.",
 "Reads only explicit sibling JSON operand ledgers. Frozen Python is never\nimported/executed. The unaccepted full R4 operands remain whole-review inputs.")
code=code.replace("all420_freshly_entered=True,all51_helpers_freshly_entered=True,\n no_unflagged397_acceptance=True", "all420_from_explicit_unaccepted_operands=True,all51_helpers_reexpanded=True,\n no_unlisted411_acceptance=True")
code=code.replace("'body-ledgers.py','helper-ledgers.py','build-census.py'", "'body-ledgers.json','helper-ledgers.json','corrections.json','prepare-r5.py','build-census.py'")
code=code.replace("'All420 and51 explicitly entered from selected source;no previous unflagged price acceptance or generator execution.'", "'All420/51 explicit unaccepted source-ledger inputs re-expanded,with Reader/vector/SDK-result-type inventory and complete schedules/quotes;no prior unlisted-row acceptance or frozen helper execution.'")
code=code.replace("The sibling body/helper ledgers are freshly authored inputs;the generator has no old-row fallback. Old JSON is read only after fresh construction,for differences and the protected exact190 catalogue.",
 "The sibling JSON body/helper ledgers explicitly preserve the unaccepted whole R4 source proposals plus the supported Reader/vector/result-type corrections. Every420/51 expansion and38 schedules is regenerated;none of the other411 rows is accepted by default. Frozen Python is read as lineage only,never imported/executed.")
code=code.replace("All420 source bodies and51 helpers rebuilt;whole independent numeric review", "All420 source bodies and51 helpers recomposed;fresh whole independent numeric review")
code=code.replace("for rev in ('r1','r2','r3'):","for rev in ('r1','r2','r3','r4'):")
needle="for rel in refs:assert (ROOT/rel).is_file(),rel"
assert needle in code
code=code.replace(needle,"refs += ['build-ci/20261106-musical-funded-legacy-adapter-r4-numeric-independent-review-r4/'+n for n in ('review.md','review.json','source-pins.json')]\nrefs += ['C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include/bit','C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include/vector']\n"+needle)
# Correction data is attached before output so no MD-only fact bypasses JSON.
needle="save('source-inventory.json',source_inventory)"
code=code.replace(needle,"packet['R5_whole_source_corrections']=json.loads((HERE/'corrections.json').read_bytes())\npacket['R5_Reader_vector_SDK_inventory']=json.loads((HERE/'source-boundaries.json').read_bytes())\nsource_inventory['R5_typed_boundaries']=packet['R5_Reader_vector_SDK_inventory']\n"+needle)
needle="save('source-inventory.json',source_inventory)"
validation="""for source_pin in packet['R5_Reader_vector_SDK_inventory']['input_pins']:
    assert pin(ROOT/source_pin['path'])==source_pin,source_pin['path']
r5_critic=read('build-ci/20261106-musical-funded-legacy-adapter-r4-numeric-independent-review-r4/review.json')
assert len(deltas)==9 and {e['site'] for e in deltas}==set(r5_critic['source_findings'])
assert not helper_deltas
for case,record in r5_critic['source_findings'].items():
    assert E[case]['body_vector']==record['necessary_body_retaining_other_proposed_components']
    assert E[case]['body_d_vector']==record['necessary_body_d_retaining_other_proposed_components']
    assert E[case]['price_coefficients']==record['necessary_price_retaining_other_proposed_components']
packet['R5_scope']='Nine supported corrections and all420/51/38/12 recompositions remain a complete new proposal,not positive acceptance of any other411 price.'
"""
code=code.replace(needle,validation+needle)
needle=" 'Each body has its complete proposed ten-component vector"
assert needle in code
paragraph=" 'R5 follows whole R4 REVISE EE53162D. ContextDecision retains its fifth family ordinary Reader conditional;the complete source inventory has ten Reader selections across five bodies. Five capture vector[] calls each retain a logical stored-base R separate from the element/field use,index transfer/subscript/HZ. The selected pinned MSVC bit header returns int:outer int-to-U64 is fixed X1 at all three callers;only the two explicit U64-to-size_t input casts retain d. Every other411 price is still an unaccepted proposed source packet. All420 expansions,51 helper parts,38 schedules,12Q terms,roles,capture,root and examples are regenerated.','',\n"
code=code.replace(needle,paragraph+needle)
needle="for rel in refs:assert (ROOT/rel).is_file(),rel"
code=code.replace(needle,"refs += ['build-ci/20261105-musical-funded-legacy-adapter-r4-census-r4/first-green-whole-source-freeze.json','C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include/cmath','C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt/corecrt_math.h']\n"+needle)
code=code.replace("'source-inventory.json','all-420-factorizations.json')", "'source-inventory.json','all-420-factorizations.json','source-boundaries.json')")
assert 'module(' not in code and 'body.ROWS' not in code
(HERE/'build-census.py').write_bytes(code.encode())
print('AUTHORED_NEW_JSON_OPERANDS',len(rows),len(previous['helpers']),len(corrections),'NO_FROZEN_HELPER_EXECUTION')
