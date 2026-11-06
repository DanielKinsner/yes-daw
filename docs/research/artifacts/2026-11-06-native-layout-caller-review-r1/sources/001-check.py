"""Own file/text/hash/integer checks only. Never imports/runs the subject,
launcher, decoder, compiler or any prior checker.
"""
from pathlib import Path
import hashlib,json,struct
P=Path(__file__).resolve().parent;R=P.parents[1]
if (P/'review.json').exists():raise ValueError('Preserve first complete own pass')
checks=[];inputs=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
 b=p.read_bytes()
 try:n=p.relative_to(R).as_posix()
 except ValueError:n=p.as_posix()
 inputs.append(dict(path=n,bytes=len(b),sha256=sha(b)));return b
def ck(n,c):
 checks.append(dict(name=n,passed=bool(c)))
 if not c:raise ValueError(n)
subject=R/'build-ci/20261106-native-layout-root-launch-r1.py';b=read(subject);s=b.decode()
ck('exact thin caller',sha(b)=='7E471316268BDE6A0D9816E9EFA248BDE9D50EA4F0FC0675286A584179497595')
ordered=['import sys','assert sys.flags.isolated and sys.flags.no_site and sys.flags.ignore_environment','assert not sys.flags.optimize and sys.maxsize == (1 << 63) - 1','assert Path(sys.executable).resolve() == host.resolve()','assert sys.version_info[:3] == (3, 13, 5)','5341746F92483A93E44C313DE830F2FBA2956F0759094404A16B2FED06C9A2ED','assert len(sys.argv) == 1','root = Path(__file__).resolve().parents[1]','8E1CA919CD6478C3B42BAD48D531B7EBA02AB4EA3BC3A4263CDD5207FD164F9B','sys.dont_write_bytecode = True','sys.path.insert(0, str(source))',"runpy.run_path(str(launcher), run_name='__main__')"]
last=-1
for text in ordered:
 n=s.find(text);ck('ordered caller guard '+text,n>last);last=n
ck('only one added search path',s.count('sys.path.insert(')==1 and 'sys.path.extend(' not in s)
ck('no subprocess in thin caller','subprocess' not in s and 'os.environ' not in s and 'exec(' not in s)
host=Path('C:/Users/SM - Dan/AppData/Local/Programs/Python/Python313/python.exe');pe=read(host)
ck('exact fixed host file',sha(pe)=='5341746F92483A93E44C313DE830F2FBA2956F0759094404A16B2FED06C9A2ED')
ck('host DOS header',len(pe)>=64 and pe[:2]==b'MZ')
o=struct.unpack_from('<I',pe,60)[0];ck('host PE bounds',o<=len(pe)-96)
ck('host AMD64 PE',pe[o:o+4]==b'PE\0\0' and struct.unpack_from('<H',pe,o+4)[0]==0x8664 and struct.unpack_from('<H',pe,o+24)[0]==0x20b)
base=R/'build-ci/20261106-native-object-layout-purpose-source-r1'
ck('pinned launcher',sha(read(base/'launch_compile.py'))=='8E1CA919CD6478C3B42BAD48D531B7EBA02AB4EA3BC3A4263CDD5207FD164F9B')
ck('pinned purpose manifest',sha(read(base/'source-pins.json'))=='7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165')
top=sorted(p.name for p in base.iterdir())
ck('exact existing top-level Python sources',sorted(p.name for p in base.iterdir() if p.suffix.lower() in ['.py','.pyw','.pyd'])==['decode_coff.py','launch_compile.py','prepare_source.py'])
ck('no top-level import packages',sorted(p.name for p in base.iterdir() if p.is_dir())==['failed-own-check-r1'])
root_review=json.loads(read(R/'build-ci/20261106-native-layout-purpose-independent-review-r1/review.json'))
ck('root separate whole review identity',root_review['verdict']=='ACCEPT_COMPILE_ONLY_PURPOSE_SOURCE' and root_review['source_pins_sha256']=='7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165')
article=read(P/'review.md').decode()
for text in ['exact nonoptimized command','earlier standard-library imports','entire host runtime','pending authorization','No caller, launcher, compiler or decoder was run','no functional findings']:
 ck('scope disclosure '+text,text.lower() in article.lower())
result=dict(schema='THIN_NATIVE_LAYOUT_ROOT_CALLER_INDEPENDENT_REVIEW_R1',verdict='ACCEPT_CALLER_FOR_EXACT_ISOLATED_COMPILE_ONLY_PURPOSE_COMMAND',subject_sha256=sha(b),exact_command=[host.as_posix(),'-I','-S',subject.relative_to(R).as_posix()],findings=[],conditions=['Exact fixed isolated nonoptimized command with no extra argv','Frozen approved source folder remains without import-shadow additions','Separate whole-purpose acceptance and root compile authorization are still required'],unobserved=['Actual host flags/version','Python DLL/stdlib/provider closure','Any caller/launcher/compiler/decoder execution or layout values','Physical stack/RSS/resource fit/Clock or supervisor facts'],top_level_source_namespace=top,checks=checks,input_pins=inputs,no_compiler_or_Clock_authority=True)
(P/'review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='OWN_CALLER_SOURCE_CHECK_EXIT0',checks=len(checks),verdict=result['verdict'],review_json_sha256=sha((P/'review.json').read_bytes()))))
