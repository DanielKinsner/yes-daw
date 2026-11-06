"""Root's staged byte reconstruction check. Does not execute archived code or ZIPs."""
from pathlib import Path, PurePosixPath
from collections import OrderedDict
import base64, hashlib, json, subprocess, sys, traceback

ROOT=Path.cwd().resolve()
assert len(sys.argv)==3
ARCHIVE=sys.argv[1]
assert ARCHIVE.startswith('docs/research/artifacts/')
OUT=ROOT/'build-ci/20261106-r16-staged-delivery-review-r1'
RESULT=ROOT/'docs/research/verification-results/2026-11-06-r16-staged-chunk-reconstruction.json'
assert not RESULT.exists(), 'First passing result is immutable'
OUT.mkdir(exist_ok=True)
ATT=OUT/('attempt-%02d'%(len(list(OUT.glob('attempt-*')))+1))
ATT.mkdir()
OWN=Path(__file__).resolve()
(ATT/'checker.py').write_bytes(OWN.read_bytes())
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
checks=0
def require(v,description):
    global checks
    checks+=1
    if not v: raise AssertionError(description)
def git(*args):
    return subprocess.run(['git',*args],cwd=ROOT,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW,check=True).stdout
def staged(name):return git('show',':'+name)
def dump(p,value):p.write_bytes((json.dumps(value,indent=2)+'\n').encode())
code=1
try:
    require(staged(OWN.relative_to(ROOT).as_posix())==OWN.read_bytes(),'staged checker identity')
    manifest_raw=staged(ARCHIVE+'/archive-manifest.json')
    require(sha(manifest_raw)==sys.argv[2],'exact selected archive manifest')
    (ATT/'manifest-input.json').write_bytes(manifest_raw)
    manifest=json.loads(manifest_raw)
    mapped={r['original_path']:r for r in manifest['files']}
    require(len(mapped)==len(manifest['files']),'unique original paths')
    def original_bytes(name):
        row=mapped[name]
        blob=staged(ARCHIVE+'/'+row['file'])
        require(len(blob)==row['bytes'] and sha(blob)==row['sha256'],'staged original wrapper identity')
        raw=base64.b64decode(row['original_raw_base64'],validate=True) if 'original_raw_base64' in row else blob
        require(len(raw)==row['original_bytes'] and sha(raw)==row['original_sha256'],'restored original wrapper identity')
        return raw
    PREFIX='build-ci/20261106-r16-review-chunked-delivery-r1/'
    freeze_raw=original_bytes(PREFIX+'first-green-delivery-freeze.json')
    require(sha(freeze_raw)=='F7F631B58E917E8CB177E53E4B54B0DBBB56AAE2C23BA5ADFA56BC7224E0508D','whole chunked delivery seal')
    freeze=json.loads(freeze_raw)
    frozen={r['path']:r for r in freeze['files']}
    expected=set(frozen)|{PREFIX+'first-green-delivery-freeze.json'}
    require({p for p in mapped if p.startswith(PREFIX)}==expected and len(expected)==37,'all37 delivery members preserved')
    for name,row in frozen.items():
        raw=original_bytes(name)
        require(len(raw)==row['bytes'] and sha(raw)==row['sha256'],'frozen delivery member')
    delivery=json.loads(original_bytes(PREFIX+'delivery.json'))
    index_raw=original_bytes(delivery['index']['path'])
    require(len(index_raw)==delivery['index']['bytes'] and sha(index_raw)==delivery['index']['sha256'],'whole extent index identity')
    index=json.loads(index_raw)
    original_root=Path(index['original_root']).resolve()
    require(original_root==ROOT/'build-ci/20261106-r16-supervisor-functional-independent-review-r1','exact original review root')
    require(index['original_firstgreen_sha256']=='E41AEAD77D8AA7D1BBED1AE6B1F0AD7FC97B7917051AE683B61A960517CAFDB8','original source review seal')
    locations={};chunks={};cache=OrderedDict()
    for desc in index['blob_chunks']:
        raw=original_bytes(desc['path'])
        require(len(raw)==desc['bytes']<16*1024*1024 and sha(raw)==desc['sha256'],'bounded staged chunk')
        chunk=json.loads(raw)
        require(chunk['schema']=='RAW_BYTE_BLOBS_ZLIB_BASE64_V1','own envelope codec')
        chunks[desc['path']]=desc
        for row in chunk['blobs']:
            require(row['id'] not in locations,'unique addressed blob')
            locations[row['id']]=desc['path']
    require(len(locations)==index['blob_count']==5181 and len(chunks)==19,'complete blob inventory')
    import zlib
    def read_blob(ident):
        loc=locations[ident]
        if loc not in cache:
            cache[loc]={r['id']:r for r in json.loads(original_bytes(loc))['blobs']}
            if len(cache)>3:cache.popitem(last=False)
        cache.move_to_end(loc)
        row=cache[loc][ident]
        packed=base64.b64decode(row['data'],validate=True)
        require(row['codec']=='zlib+base64' and len(packed)==row['compressed_bytes'] and sha(packed)==row['compressed_sha256'],'packed envelope identity')
        require(0<row['raw_bytes']<=8*1024*1024,'bounded envelope raw extent')
        dec=zlib.decompressobj();raw=dec.decompress(packed,row['raw_bytes']+1)
        require(dec.eof and not dec.unused_data and not dec.unconsumed_tail and dec.flush()==b'','complete single own envelope')
        require(len(raw)==row['raw_bytes'] and sha(raw)==row['raw_sha256']==ident,'decoded blob identity')
        return raw
    names=set();seen=set();total=0;local_members=0
    for row in index['files']:
        name=PurePosixPath(row['relative_path'])
        require(not name.is_absolute() and '..' not in name.parts and '\\' not in str(name),'safe original review path')
        require(str(name) not in names,'unique original member');names.add(str(name))
        source=(original_root/str(name)).resolve()
        require(source.is_relative_to(original_root) and not source.is_symlink(),'exact immutable original path')
        at=0;whole=hashlib.sha256()
        with source.open('rb') as stream:
            for span in row['extents']:
                require(span['offset']==at and span['bytes']>0,'gapless original spans')
                count=0;held=hashlib.sha256()
                for ident in span['blob_ids']:
                    raw=read_blob(ident);seen.add(ident)
                    require(stream.read(len(raw))==raw,'direct original byte equality')
                    count+=len(raw);whole.update(raw);held.update(raw)
                require(count==span['bytes'] and held.hexdigest().upper()==span['sha256'],'complete original span')
                at+=count
            require(stream.read(1)==b'' and at==row['bytes'] and whole.hexdigest().upper()==row['sha256'],'whole original exact EOF/hash')
        total+=at
        if row['zip'] is not None:
            z=row['zip'];require(len(z['members'])==z['entries'],'raw ZIP local record inventory')
            for member in z['members']:
                spans=[row['extents'][i] for i in member['extent_indexes']]
                require(spans[0]['offset']==member['local_offset'] and sum(s['bytes'] for s in spans)==member['local_record_bytes'] and all(s['member']==member['index'] for s in spans),'complete local ZIP raw extent membership')
                local_members+=1
    require(names=={p.relative_to(original_root).as_posix() for p in original_root.rglob('*') if p.is_file()} and len(names)==42,'whole original review membership')
    require(seen==set(locations),'all encoded blobs have original sponsors')
    require(total==index['original_total_bytes']==540637273 and local_members==3882,'complete original byte and raw ZIP record counts')
    meta=index['metadata_input_identities'];raw=original_bytes(meta['path'])
    require(len(raw)==meta['bytes'] and sha(raw)==meta['sha256'],'metadata identity file')
    metadata=json.loads(raw);pins_raw=(original_root/'source-pins.json').read_bytes()
    require(metadata['rows']==json.loads(pins_raw)['complete_inputs'] and len(metadata['rows'])==1381 and metadata['original_source_pins_sha256']==sha(pins_raw),'all1381 input identity rows retained')
    require(sha((ROOT/'AGENTS.md').read_bytes())==sha((ROOT/'CLAUDE.md').read_bytes())=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77','protected instructions')
    require(sha((ROOT/'tests/musical_audio_call_recipe_tests.cpp').read_bytes())=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C','protected unrun application test')
    git('diff','--cached','--check')
    result=dict(scope='ROOT_STAGED_LOSSLESS_BYTE_RECONSTRUCTION_ONLY',archive_manifest_sha256=sys.argv[2],original_review_files=42,original_bytes=total,raw_ZIP_local_records=local_members,chunk_files=19,unique_blobs=5181,metadata_input_rows=1381,target_execution=False,ZIP_member_inflation_extraction_or_execution=False,numeric_resource_native_binding_limit_Data_GO=False,checker_sha256=sha(OWN.read_bytes()),checks=checks,failed=0,exit_code=0)
    dump(RESULT,result)
    code=0
except BaseException:
    (ATT/'stderr.txt').write_text(traceback.format_exc(),encoding='utf-8',newline='\n')
    result=dict(scope='ROOT_STAGED_BYTE_RECONSTRUCTION_ONLY',checks=checks,failed=1,exit_code=1,target_execution=False)
dump(ATT/'result.json',result)
(ATT/'stdout.txt').write_text(json.dumps(result)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(result))
raise SystemExit(code)
