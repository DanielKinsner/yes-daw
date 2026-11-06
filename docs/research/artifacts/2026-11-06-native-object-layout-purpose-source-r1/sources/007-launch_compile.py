"""UNRUN proposal: one separately authorized hidden compile-only purpose.
No shell, link, target executable, target main, native run or ambient environment.
Root must first write the separately reviewed compile-authorization.json.
"""
from pathlib import Path
import ctypes,hashlib,json,os,struct,subprocess,time

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
RUN=HERE/'execution-r1'

def require(ok,reason):
    if not ok:raise ValueError(reason)

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda:source.read(65536),b''):h.update(block)
    return h.hexdigest().upper()

def json_file(path,maximum=1024*1024):
    require(0<path.stat().st_size<=maximum,'JSON_INPUT_EXTENT')
    return json.loads(path.read_bytes())

def main():
    require(os.name=='nt' and __debug__,'WINDOWS_UNOPTIMIZED_LAUNCHER')
    require(not RUN.exists(),'PRESERVE_EVERY_ATTEMPT')
    pins_path=HERE/'source-pins.json'
    authority=json_file(HERE/'compile-authorization.json')
    require(set(authority)=={'kind','source_pins_sha256','review_path','review_sha256'},'AUTHORITY_FIELDS')
    require(authority['kind']=='ROOT_SEPARATE_COMPILE_ONLY_GO','ROOT_PURPOSE_GO_REQUIRED')
    require(digest(pins_path)==authority['source_pins_sha256'],'APPROVED_SOURCE_PINS')
    review_relative=Path(authority['review_path'])
    require(not review_relative.is_absolute() and not review_relative.drive and not review_relative.root and '..' not in review_relative.parts and len(str(review_relative))<=512,'REVIEW_PATH')
    review=ROOT/review_relative
    require(digest(review)==authority['review_sha256'],'EXACT_REVIEW_IDENTITY')
    record=json_file(review)
    require(record.get('verdict')=='ACCEPT_COMPILE_ONLY_PURPOSE_SOURCE' and record.get('source_pins_sha256')==authority['source_pins_sha256'],'INDEPENDENT_PURPOSE_REVIEW')
    pins=json_file(pins_path)
    def assert_pins():
        require(digest(pins_path)==authority['source_pins_sha256'],'PINS_STABLE')
        for row in pins['files']+pins['inputs']:
            path=Path(row['path'])
            if not path.is_absolute():path=ROOT/path
            require(path.stat().st_size==row['bytes'] and digest(path)==row['sha256'],'SOURCE_OR_TOOL_IDENTITY')
    assert_pins()
    profile=json_file(HERE/'profile.json')
    compiler=Path(profile['compiler']['path'])
    pe=compiler.read_bytes()
    require(len(pe)>=64 and pe[:2]==b'MZ','COMPILER_PE')
    offset=struct.unpack_from('<I',pe,60)[0]
    require(offset<=len(pe)-96 and pe[offset:offset+4]==b'PE\0\0','COMPILER_PE_RANGE')
    require(struct.unpack_from('<H',pe,offset+4)[0]==0x8664 and struct.unpack_from('<H',pe,offset+24)[0]==0x20b and struct.unpack_from('<H',pe,offset+92)[0]==3,'AMD64_CONSOLE_COMPILER')
    RUN.mkdir();temp=RUN/'compiler-temp';temp.mkdir()
    receipt_path=RUN/'receipt.json'
    receipt=dict(schema='NATIVE_LAYOUT_COMPILE_ONLY_PURPOSE_ATTEMPT_R1',first_green=False,source_pins_sha256=authority['source_pins_sha256'],independent_review_sha256=authority['review_sha256'],no_Clock_execution=True,no_link=True,stdin_null=True,hidden_console=True,fresh_fixed_environment=True,timeout_seconds=120,output_limits_are_refusal_observations_not_a_hard_allocator_or_disk_certificate=True)
    def save():receipt_path.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
    obj=RUN/'layout.obj';deps=RUN/'dependencies.json'
    replacements={'<SOURCE>':str(HERE/'layout.cpp'),'<OBJECT>':str(obj),'<DEPS>':str(deps),'<REPO>':str(ROOT)}
    args=profile['compiler']['arguments'][:]
    for i,arg in enumerate(args):
        for token,value in replacements.items():arg=arg.replace(token,value)
        args[i]=arg
    require('/c' in args and not any(x in args for x in ['/link','/Fe','/LD','/LDd']), 'COMPILE_ONLY_ARGUMENTS')
    env=profile['environment'].copy();env['TEMP']=str(temp);env['TMP']=str(temp)
    receipt['command']=[str(compiler)]+args
    receipt['environment_keys_only']=sorted(env)
    save()
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.SetErrorMode.argtypes=[ctypes.c_uint];kernel.SetErrorMode.restype=ctypes.c_uint
    old_mode=kernel.SetErrorMode(0x8003);kernel.SetErrorMode(old_mode|0x8003)
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=subprocess.SW_HIDE
    start=time.monotonic();failure=None;settled=False;child=None
    try:
        with (RUN/'compiler.stdout.log').open('xb') as stdout,(RUN/'compiler.stderr.log').open('xb') as stderr:
            child=subprocess.Popen([str(compiler)]+args,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,stdout=stdout,stderr=stderr,creationflags=subprocess.CREATE_NO_WINDOW,startupinfo=startup)
            while child.poll() is None:
                elapsed=time.monotonic()-start
                observed=sum(p.stat().st_size for p in [RUN/'compiler.stdout.log',RUN/'compiler.stderr.log'])
                if elapsed>120 or observed>1024*1024 or (obj.exists() and obj.stat().st_size>16*1024*1024):
                    failure='TIMEOUT' if elapsed>120 else 'OUTPUT_EXTENT_REFUSAL'
                    child.kill()
                    break
                time.sleep(0.05)
            try:receipt['compiler_raw_exit']=child.wait(timeout=5);settled=True
            except subprocess.TimeoutExpired:failure='UNOBSERVED_COMPILER_SETTLEMENT'
    finally:
        if child is not None and child.poll() is None:
            try:
                child.kill();receipt['compiler_raw_exit']=child.wait(timeout=5);settled=True
            except Exception:
                failure='UNOBSERVED_COMPILER_SETTLEMENT'
        kernel.SetErrorMode(old_mode)
        receipt['seconds']=time.monotonic()-start
        receipt['compiler_process_settlement_observed']=settled
        receipt['failure']=failure
        receipt['compiler_internal_helpers_or_temp_peak_remain_unqualified_on_abnormal_exit']=True
        save()
    require(settled and failure is None and receipt['compiler_raw_exit']==0,'COMPILE_NOT_QUALIFIED')
    require(sum(p.stat().st_size for p in [RUN/'compiler.stdout.log',RUN/'compiler.stderr.log'])<=1024*1024,'FINAL_DIAGNOSTIC_EXTENT')
    assert_pins()
    dependency=json_file(deps)
    data=dependency['Data']
    canonical=lambda p:str(Path(p).resolve()).casefold()
    approved={canonical((ROOT/Path(r['path'])) if not Path(r['path']).is_absolute() else Path(r['path'])) for r in pins['inputs']+pins['files']}
    require(canonical(data['Source'])==canonical(HERE/'layout.cpp'),'DEPENDENCY_SOURCE')
    includes=data['Includes']
    require(isinstance(includes,list) and 1<=len(includes)<=1024 and all(isinstance(p,str) and len(p)<=1024 and canonical(p) in approved for p in includes),'ADMITTED_HEADER_DEPENDENCIES')
    require(canonical(HERE/'layout-types.h') in {canonical(p) for p in includes},'REPLICA_HEADER_USED')
    from decode_coff import decode
    result=decode(obj,json_file(HERE/'schema.json'))
    (RUN/'layout-values.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    assert_pins()
    receipt['include_count']=len(includes);receipt['object_sha256']=digest(obj);receipt['layout_values_sha256']=digest(RUN/'layout-values.json')
    receipt['first_green']=True;save()
    (RUN/'first-green-whole-source-freeze.json').write_text(json.dumps(dict(source_pins_sha256=authority['source_pins_sha256'],review_sha256=authority['review_sha256'],object_sha256=receipt['object_sha256'],scope='Whole purpose CPP/replicas/profile/schema/decoder/launcher/checker/docs frozen; compile-layout facts only'),indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(status='COMPILE_ONLY_LAYOUT_QUALIFIED',cells=result['cells'],object_sha256=receipt['object_sha256'],no_Clock_execution=True)))

if __name__=='__main__':
    try:main()
    except Exception as error:
        print(json.dumps(dict(status='PURPOSE_REFUSAL',reason=str(error)[:160] if type(error) is ValueError else type(error).__name__)))
        raise SystemExit(1)
