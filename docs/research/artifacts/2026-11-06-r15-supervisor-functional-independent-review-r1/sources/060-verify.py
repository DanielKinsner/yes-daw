from pathlib import Path
import json,hashlib,re,ntpath,os
ROOT=Path(__file__).resolve().parents[2]
P=ROOT/'build-ci/20261105-clock-oracle-r9-runner-census'
S=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype'
x=json.loads((P/'census.json').read_text(encoding='utf-8'));pins=json.loads((P/'source-pins.json').read_text(encoding='utf-8'));m=json.loads((S/'source-manifest.json').read_text(encoding='utf-8'))
checks=[]
def check(n,a,b):
 checks.append({'name':n,'actual':a,'expected':b,'pass':a==b})
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
for f,h in [('census.md','9F43A03908B5BDE7CC67AFE9EA48D46B18217AD6BCB903B804154FF21C368D78'),('census.json','7F8425F9982EFEC1C5218AFD95F3A8E443C172DEC61112E5F9008306EA6A654E'),('source-pins.json','840645DFD6FFD12177A915081C9757F1BB07943FC8384494A9F4C110D1AE3599')]:check('packet '+f,digest(P/f),h)
for z in pins['target_five']:
 p=ROOT/z['path'];check('subject bytes '+z['path'],p.stat().st_size,z['bytes']);check('subject SHA '+z['path'],digest(p),z['sha256'])
for z in m['inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else ROOT/p
 check('input '+z['path'],(p.stat().st_size,digest(p)),(z['bytes'],z['sha256']))
check('input count',len(m['inputs']),1186);check('inputbytes',sum(z['bytes'] for z in m['inputs']),458254003)
ns_actual=[]
for z in m['closed_directory_inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else ROOT/p
 rows=[];files=[];links=[]
 def visit(q):
  for ent in os.scandir(q):
   rel=Path(ent.path).relative_to(p).as_posix();rows.append(rel)
   if ent.is_symlink() or ent.stat(follow_symlinks=False).st_file_attributes & 0x400:links.append(rel)
   elif ent.is_dir(follow_symlinks=False):
    if z['recursive']:visit(Path(ent.path))
   else:files.append(rel)
 visit(p);check('namespace rows '+z['path'],len(rows),z['entry_count']);check('namespace files '+z['path'],sorted(files),sorted(z['files']));check('namespace reparse '+z['path'],links,[]);ns_actual.append((len(rows),len(files)))
check('namespace aggregate',[len(ns_actual),sum(z[0] for z in ns_actual),sum(z[1] for z in ns_actual)],[27,1058,1012])
text=(S/'run-clock.ps1').read_text(encoding='utf-8');starts=list(re.finditer(r'^function ([A-Za-z-]+)',text,re.M));names=[q[1] for q in starts]
check('helper name coverage',sorted(set(names)),sorted(set(r['name'] for r in x['helper_rows'])))
check('helper count',len(names),62)
source_lines=text.splitlines()
for n,l in pins['own_identity_audit']['helper_locations'].items():check('helper location '+n,source_lines[l['start']-1].startswith('function '+n),True)
for r in x['helper_rows']:
 if 'vector' in r:check('vector '+r['name']+' '+r.get('path',''),sum(r['vector'].values()),r.get('localUnits',r.get('units')))
 if 'fixedVector' in r:check('fixedvector '+r['name']+' '+r.get('path',''),sum(r['fixedVector'].values()),r.get('fixedUnits',r.get('fixedLocalUnits')))
raw=x['raw_lexical_derivation'];check('raw fixed',sum(raw['fixed_groups'].values()),106);check('raw numeric fixed',sum(raw['numeric_fixed_groups'].values()),112);check('raw sum',sum([234,111,61,38,32,36]),512)
check('raw units',476569+98009+4194304+2*65536+20+20+21,4900015)
check('bootstrap definitions',sum(r['units'] for r in x['fixed_bootstrap_definition_rows']),48028)
check('main old rows',sum(r['units'] for r in x['main_error_rows_lineage']['old_rows']),124455);check('main deltas',sum(x['main_error_rows_lineage']['delta_groups'].values()),1600)
check('composition sum',sum(x['composition'].values()),5041187603)
# Independent explicit equations from source loops and profiles.
fence=len(m['inputs'])+len(m['sources'])+1;chunks=sum((z['bytes']+65535)//65536 for z in m['inputs'])+sum((z['bytes']+65535)//65536 for z in m['sources'])+((S/'source-manifest.json').stat().st_size+65535)//65536
check('fence identities',fence,1191);check('fence chunks',chunks,7807)
check('hash calls',8*fence+136,9664);check('hash chunks',8*chunks+10536,72992)
check('namespace guard rows',sum(z['entry_count']+1 for z in m['closed_directory_inputs']),1085)
check('namespace Dupper',sum(z['entry_count']+2 if z['recursive'] else 1 for z in m['closed_directory_inputs']),692)
def ent(d,e):return 56+4725*d+204*e+64*512*e+129+64*512-22
ns=27*(56+129+64*512-22)+4725*692+(204+64*512)*1085
C={
'fixedBootstrapDefinitions':48028,'fixedMainCatchAndSourceCallers':124455+1600,'InitializeControllerWithSevenFixedHashes':2136+53*14*512+7*3415+45*379,
'completeGetHashHelpers':4865*9664+65*72992,'remainingNoLinkHelpers':415*4570,'allResolveHelpers':(10*1186+8*27+1187+4)*32863,
'allDirectoryWalkerHelpers':784*ent(1,7)+8*ns+2*ent(79,78)+2*ent(61,60)+2*ent(1,4)+ent(1,1),'stageByteOwnRows':784*532,
'pinsOwnRows':8*(87+50*27+38*1085+63*4+44*1186+27*(4+1186)),
'FileSetNormalClosedSlots':224*100+83*8351+78*8351+64*512*(8351+8351),
'childOwnRowsWithAllPollsCleanup':3*1044811+74*40+53*512*(40+3*8),
'threeFencedWrapperOwnRows':3*107+1+2*69,'84CompilerReplacementHelpers':53*(84*512+84*512+28*(6+6+7))+217*84+18*18+61*38+66+4*86,
'twoOwnedCopyHelpers':2*9425+70*514,'twoOwnedMutationHelpers':2*263,'fourPEHeaderHelpers':4*333,'threeOrdinaryConsolePE':3*31,'oneUnexpectedConsolePERefusalReplacementSurplus':93-31,
'predecessorDescriptor':2+4988+32824+3448+300+35+38,'predecessorMap':3557+62,'fourBoundSignedIntegerHelpers':4*110,
'twoNormalWorkReservations':2*20,'twoNormalRetainedReservations':2*16,'normalAttempt':6,'normalLastLegalPoll':11,
'rootASCIIValidation':53*512+49,'28CompilerArgumentASCIIValidations':28*(53*512+49),
'elevenMeasurementOwnRowsAndDefaults':11*103+7,'fourSaveReceiptOwnRowsExcludingMeasurementHelper':4*(684-103),'twoWriteJsonOwnRowsExcludingMeasurementHelper':2*59+2+4,
'thirteenDocumentOwnRows':13*(64*512+91),
'sevenOnceEncodeOwnRows':38*((319-1)+4*(23635-1)+(1048576-1)+(262760-1))+7*175,
'bootstrapBoundedRead':197,'twoReadJsonOwnRowsExcludingDependencyShape':2*303+2+8+25+3+1+2+1,'dependencyShapeAll4MiBUnits':19,
'dependencyOtherOwnRows':421+64*512+1187*(69+64*512),
'twoChildResultOwnRowsWith14AdmittedControls':2*21557+38*131072+14*12630+8*14+15*2,
'oneAttemptedBadControlPrefixBeforeRefusal':11886+8,'twoBuildArtifactOwnRows':2*(47+98*4),'twoReferenceOwnRows':2*(54+39*60+98*56),
'sameReferencesIncluding56normalIdentities':30+51*56,'sevenOtherNormalIdentityHelpers':7*24,'twoUnexpectedIdentityRefusalReplacementSurpluses':2*(93-24),'failureInventoryOwnRows':193+64*78,
'allRawJsonLexicalHelpers':512*4900015+106*22,
'twoUnexpectedFileSetRefusalReplacementSurpluses':2*(201+32862*1191+32864*1191),
}
def join(k,s):return 66+87*k+64*s
def fserr(n,o,mis,unknown):return 169+90*n+87*o+64*512*(n+o)+join(len(mis),sum(map(len,mis)))+join(len(unknown),sum(map(len,unknown)))
C['threeActualExpectedFileSetRefusals']=fserr(56,55,['references/native-inverse/r48000-map5.json'],[])+fserr(56,57,[],['unlisted.bin'])+fserr(56,57,[],[])
def visitor(g):return 185*g['maps']+190*g['arrays']+223*g['keys']+31*g.get('map_separators',g['keys']-g['maps'])+212*g['key_units']+21*g.get('array_elements',0)+31*g.get('array_separators',0)+184*g['strings']+148*g['string_units']+455*g['integers']+115*g['booleans']
for name,g in x['graphs'].items():
 if name=='failure':g=dict(g,array_elements=78,array_separators=77,map_separators=397-79)
 if name=='marker':g=dict(g,array_elements=0,array_separators=0,map_separators=5)
 check('graph visitor '+name,visitor(g),g['visitor'])
C['allRecursiveVisitorPasses']=31586+4*1947224+5*131998376+7193129
check('composition keys',sorted(C),sorted(x['composition']))
for k,v in C.items():check('composition '+k,v,x['composition'][k])
check('known byte subtotal',sum(x['controlled_payload']['known_normal_input_byte_requests'].values())+sum(x['controlled_payload']['known_document_byte_upper_requests'].values())+sum(x['controlled_payload']['known_fixed_array_requests'].values())+sum(x['controlled_payload']['configured_stream_buffer_upper_requests'].values()),6626082)
check('normal decoded storage',2*(476569+98009+4194304+2*65536)+2*5,9799918)
check('reader witness lower',2*4194304+2*2097152+2*4194304+65536,21037056)
check('clone prime sums',7+4*462+8836+553,11244)
md=(P/'census.md').read_text(encoding='utf-8')
for k,v in x['composition'].items():check('MD composition '+k,re.search(r'`'+re.escape(k)+r'`\s*\|\s*([\d,]+)',md).group(1).replace(',',''),str(v))
report={'scope':'ordinary file/hash/text/arithmetic review only; no target parser/import/program/compiler/stage execution','checks':checks,'total':len(checks),'failures':[q for q in checks if not q['pass']]}
(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/mechanical-evidence.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'failures':report['failures']},indent=2))
