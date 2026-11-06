"""Lossless byte packaging of the immutable R16 review; no ZIP extraction.

ZIP local headers, compressed payloads and directory/trailer are separate raw
extents. Their bytes are content-addressed, deduplicated, zlib/Base64 encoded,
and stored in JSON files under16MiB. Ordered extents reconstruct original ZIPs.
"""
from pathlib import Path
import base64
import hashlib
import json
import shutil
import struct
import sys
import traceback
import zlib

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
ORIGINAL=ROOT/'build-ci/20261106-r16-supervisor-functional-independent-review-r1'
EXPECTED_FREEZE='E41AEAD77D8AA7D1BBED1AE6B1F0AD7FC97B7917051AE683B61A960517CAFDB8'
BLOCK=8*1024*1024
CHUNK_LIMIT=16*1024*1024
TARGET_PAYLOAD=15*1024*1024
if (OUT/'delivery.json').exists():
    raise SystemExit('Complete writer result exists; no rerun or rewrite.')
ATT=OUT/'attempts'/('write-%02d'%(1+len(list((OUT/'attempts').glob('write-*')))))
PACKAGE=ATT/'package'
PACKAGE.mkdir(parents=True)
shutil.copyfile(__file__,ATT/'writer-input.py')

def sha(b):return hashlib.sha256(b).hexdigest().upper()
def dump(p,v):p.write_text(json.dumps(v,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def file_sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        while b:=f.read(BLOCK):h.update(b)
    return h.hexdigest().upper()
def rec(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':file_sha(p)}
checks=[]
def test(n,v):
    checks.append({'name':n,'pass':bool(v)})
    if not v:raise AssertionError(n)

chunks=[]
blobs={}
pending=[]
payload_length=0
extent_count=0
extent_bytes=0
unique_raw_bytes=0
def flush():
    global pending,payload_length
    if not pending:return
    name='blobs-%03d.json'%(len(chunks)+1)
    target=PACKAGE/name
    dump(target,{'schema':'RAW_BYTE_BLOBS_ZLIB_BASE64_V1','blobs':pending})
    test('JSON chunk under16MiB '+name,target.stat().st_size<CHUNK_LIMIT)
    chunks.append(rec(target))
    pending=[];payload_length=0
def add_blob(raw):
    global payload_length,extent_count,extent_bytes,unique_raw_bytes
    test('bounded nonempty raw blob',0<len(raw)<=BLOCK)
    extent_count+=1;extent_bytes+=len(raw)
    ident=sha(raw)
    if ident in blobs:
        test('same hash has same length',blobs[ident]['raw_bytes']==len(raw))
        return ident
    compressed=zlib.compress(raw,1)
    data=base64.b64encode(compressed).decode('ascii')
    if pending and payload_length+len(data)>TARGET_PAYLOAD:flush()
    row={'id':ident,'raw_bytes':len(raw),'raw_sha256':ident,'codec':'zlib+base64','compressed_bytes':len(compressed),'compressed_sha256':sha(compressed),'data':data}
    pending.append(row);payload_length+=len(data)
    blobs[ident]={'raw_bytes':len(raw),'chunk':len(chunks)+1}
    unique_raw_bytes+=len(raw)
    return ident
def extent(f,start,length,kind,member=None):
    f.seek(start)
    ids=[];remaining=length;held=hashlib.sha256()
    while remaining:
        raw=f.read(min(remaining,BLOCK))
        test('raw extent exact read',len(raw)==min(remaining,BLOCK))
        held.update(raw);ids.append(add_blob(raw));remaining-=len(raw)
    return {'offset':start,'bytes':length,'sha256':held.hexdigest().upper(),'kind':kind,'member':member,'blob_ids':ids}

def zip_extents(path):
    """Read ZIP container records only. Never inflate or extract a ZIP member."""
    length=path.stat().st_size
    rows=[];members=[]
    with path.open('rb') as f:
        f.seek(max(0,length-65557));tail=f.read()
        where=tail.rfind(b'PK\x05\x06')
        test('ZIP EOCD exists',where>=0)
        end=length-len(tail)+where
        e=struct.unpack_from('<4s4H2IH',tail,where)
        _,disk,cd_disk,disk_entries,count,cd_bytes,cd_at,comment=e
        test('single disk nonZip64 fixed EOCD',disk==cd_disk==0 and disk_entries==count and count<65535 and end+22+comment==length and cd_at+cd_bytes==end)
        f.seek(cd_at);central=f.read(cd_bytes)
        at=0;infos=[]
        for i in range(count):
            test('central header in bounds',at+46<=len(central))
            c=struct.unpack_from('<4s6H3I5H2I',central,at)
            signature,made,need,flags,method,clock,date,crc,csize,usize,nlen,xlen,clen,start_disk,attrs,external,local=c
            test('central member no descriptor/Zip64/spanning',signature==b'PK\x01\x02' and not flags&8 and start_disk==0 and csize<0xffffffff and usize<0xffffffff and local<0xffffffff and at+46+nlen+xlen+clen<=len(central))
            name=central[at+46:at+46+nlen]
            infos.append({'index':i,'name_hex':name.hex().upper(),'name':name.decode('utf-8' if flags&0x800 else 'cp437'),'flags':flags,'method':method,'crc32':crc,'compressed_bytes':csize,'uncompressed_bytes':usize,'local_offset':local,'central_offset':cd_at+at,'central_bytes':46+nlen+xlen+clen})
            at+=46+nlen+xlen+clen
        test('central bytes exactly covered',at==len(central))
        cursor=0
        for info in sorted(infos,key=lambda x:x['local_offset']):
            local=info['local_offset']
            test('local records ordered nonoverlapping',local>=cursor)
            if local>cursor:rows.append(extent(f,cursor,local-cursor,'zip-inter-record-span'))
            f.seek(local);header=f.read(30)
            test('local fixed header exact',len(header)==30)
            a=struct.unpack('<4s5H3I2H',header)
            sig,ver,flags,method,clock,date,crc,csize,usize,nlen,xlen=a
            header_bytes=30+nlen+xlen
            test('local/central metadata correspond',sig==b'PK\x03\x04' and flags==info['flags'] and method==info['method'] and crc==info['crc32'] and csize==info['compressed_bytes'] and usize==info['uncompressed_bytes'] and local+header_bytes+csize<=cd_at)
            f.seek(local+30);name=f.read(nlen)
            test('local/central member name exact',name.hex().upper()==info['name_hex'])
            first=len(rows)
            rows.append(extent(f,local,header_bytes,'zip-local-header',info['index']))
            if csize:rows.append(extent(f,local+header_bytes,csize,'zip-member-compressed-payload',info['index']))
            info['local_record_bytes']=header_bytes+csize
            info['local_record_sha256']=None
            # Hash complete local record from raw source without member inflation.
            f.seek(local);held=hashlib.sha256();remain=header_bytes+csize
            while remain:
                block=f.read(min(BLOCK,remain));test('local record hash exact read',len(block)==min(BLOCK,remain));held.update(block);remain-=len(block)
            info['local_record_sha256']=held.hexdigest().upper()
            info['extent_indexes']=list(range(first,len(rows)))
            members.append(info);cursor=local+header_bytes+csize
        test('locals before central directory',cursor<=cd_at)
        if cursor<cd_at:rows.append(extent(f,cursor,cd_at-cursor,'zip-pre-central-span'))
        rows.append(extent(f,cd_at,cd_bytes,'zip-central-directory'))
        rows.append(extent(f,end,length-end,'zip-end-directory-and-trailer'))
    return rows,{'members':members,'central_directory_offset':cd_at,'central_directory_bytes':cd_bytes,'eocd_offset':end,'entries':count,'raw_container_only_no_extraction':True}

code=0
try:
    original_files=sorted(p for p in ORIGINAL.rglob('*') if p.is_file())
    original_records=[rec(p) for p in original_files]
    dump(ATT/'complete-original-inputs.json',{'original_freeze':EXPECTED_FREEZE,'files':original_records,'raw_bytes_retained_at_immutable_original_paths':True,'no_1381_input_rearchive':True})
    test('original42 files',len(original_files)==42)
    test('original complete firstgreen identity',file_sha(ORIGINAL/'first-green-review-freeze.json')==EXPECTED_FREEZE)
    frozen=json.loads((ORIGINAL/'first-green-review-freeze.json').read_bytes())
    for row in frozen['files']:
        p=ROOT/row['path'];test('original frozen member '+row['path'],p.stat().st_size==row['bytes'] and file_sha(p)==row['sha256'])
    archived=[]
    for path,row in zip(original_files,original_records):
        name=path.relative_to(ORIGINAL).as_posix()
        if path.suffix=='.zip':
            spans,zip_info=zip_extents(path)
        else:
            with path.open('rb') as f:spans=[extent(f,0,path.stat().st_size,'whole-file')] if path.stat().st_size else []
            zip_info=None
        archived.append({'relative_path':name,'original_path':row['path'],'bytes':row['bytes'],'sha256':row['sha256'],'extents':spans,'zip':zip_info})
    flush()
    current_pins=json.loads((ORIGINAL/'source-pins.json').read_bytes())
    identity_rows=current_pins['complete_inputs']
    dump(PACKAGE/'metadata-input-identities.json',{'source_original_file':'source-pins.json','original_source_pins_sha256':file_sha(ORIGINAL/'source-pins.json'),'rows':identity_rows,'row_count':len(identity_rows),'these_are_identity_metadata_no_second_raw_rearchive':True})
    metadata_rec=rec(PACKAGE/'metadata-input-identities.json')
    index={'schema':'R16_FROZEN_REVIEW_RAW_EXTENT_DELIVERY_V1','original_root':str(ORIGINAL),'original_firstgreen_sha256':EXPECTED_FREEZE,'original_file_count':len(archived),'original_total_bytes':sum(x['bytes'] for x in archived),'raw_block_max':BLOCK,'json_chunk_max_exclusive':CHUNK_LIMIT,'blob_count':len(blobs),'extent_count':extent_count,'extent_bytes_before_deduplication':extent_bytes,'unique_raw_extent_bytes':unique_raw_bytes,'deduplicated_raw_extent_bytes':extent_bytes-unique_raw_bytes,'zip_record_strategy':'EXACT_LOCAL_HEADER_PLUS_COMPRESSED_MEMBER_PAYLOAD_PLUS_CENTRAL_AND_TRAILER_RAW_SPANS;NO_ZIP_EXTRACTION','blob_chunks':chunks,'metadata_input_identities':metadata_rec,'files':archived,'source_only':True,'target_execution':False}
    dump(PACKAGE/'archive-index.json',index)
    test('all package files below16MiB',all(p.stat().st_size<CHUNK_LIMIT for p in PACKAGE.glob('*') if p.is_file()))
    dump(OUT/'delivery.json',{'package':PACKAGE.relative_to(ROOT).as_posix(),'index':rec(PACKAGE/'archive-index.json'),'writer_attempt':ATT.relative_to(ROOT).as_posix(),'independent_check':'SEPARATE_VERIFICATION_JSON_AND_FIRST_GREEN_DELIVERY_FREEZE_ARE_REQUIRED','original_freeze':EXPECTED_FREEZE,'target_execution':False})
except BaseException:
    code=1;(ATT/'stderr.txt').write_text(traceback.format_exc(),encoding='utf-8',newline='\n')
summary={'exit_code':code,'checks':len(checks),'failed':sum(not x['pass'] for x in checks),'complete_original_inputs':'complete-original-inputs.json','source_only':True,'target_execution':False}
dump(ATT/'checks.json',{'checks':checks,'summary':summary})
(ATT/'stdout.txt').write_text(json.dumps(summary)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(summary))
raise SystemExit(code)
