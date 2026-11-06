"""Fresh independent streaming byte reconstruction verifier; no ZIP extraction.

Only own JSON/zlib/Base64 envelopes are decoded. ZIP container bytes remain raw.
Compares all reconstructed bytes directly with each immutable original and SHA.
"""
from pathlib import Path
from collections import OrderedDict
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
FIRST=OUT/'first-green-delivery-freeze.json'
if FIRST.exists():raise SystemExit('Complete first green frozen; no rerun/rewrite.')
ATT=OUT/'attempts'/('check-%02d'%(1+len(list((OUT/'attempts').glob('check-*')))))
ATT.mkdir(parents=True)
shutil.copyfile(__file__,ATT/'checker-input.py')
shutil.copyfile(OUT/'write_delivery.py',ATT/'writer-input.py')

def sha(b):return hashlib.sha256(b).hexdigest().upper()
def file_sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        while block:=f.read(8*1024*1024):h.update(block)
    return h.hexdigest().upper()
def dump(p,v):p.write_text(json.dumps(v,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def rec(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':file_sha(p)}
checks=[]
def test(n,v):
    checks.append({'name':n,'pass':bool(v)})
    if not v:raise AssertionError(n)

cached_chunks=OrderedDict()
locations={}
def blob(ident):
    location=locations[ident]
    if location not in cached_chunks:
        value=json.loads((ROOT/location).read_bytes())
        cached_chunks[location]={x['id']:x for x in value['blobs']}
        if len(cached_chunks)>3:cached_chunks.popitem(last=False)
    cached_chunks.move_to_end(location)
    row=cached_chunks[location][ident]
    packed=base64.b64decode(row['data'],validate=True)
    test('compressed blob hash '+ident,len(packed)==row['compressed_bytes'] and sha(packed)==row['compressed_sha256'])
    decoder=zlib.decompressobj()
    raw=decoder.decompress(packed,row['raw_bytes']+1)
    test('bounded complete zlib own envelope '+ident,decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and len(raw)==row['raw_bytes'] and len(raw)<=8*1024*1024)
    test('raw extent blob hash '+ident,sha(raw)==ident==row['raw_sha256'])
    return raw

code=0
try:
    delivery=json.loads((OUT/'delivery.json').read_bytes())
    package=ROOT/delivery['package']
    index_path=ROOT/delivery['index']['path']
    test('writer index identity',index_path.stat().st_size==delivery['index']['bytes'] and file_sha(index_path)==delivery['index']['sha256'])
    index=json.loads(index_path.read_bytes())
    original=Path(index['original_root'])
    input_files=[rec(p) for p in sorted(OUT.rglob('*')) if p.is_file() and ATT not in p.parents]
    dump(ATT/'complete-inputs.json',{'new_delivery_inputs':input_files,'original42':index['files'],'raw_inputs_remain_immutable_at_originals':True})
    test('original unchanged firstgreen',file_sha(original/'first-green-review-freeze.json')==index['original_firstgreen_sha256']=='E41AEAD77D8AA7D1BBED1AE6B1F0AD7FC97B7917051AE683B61A960517CAFDB8')
    for desc in index['blob_chunks']:
        path=ROOT/desc['path']
        test('bounded chunk identity '+path.name,path.stat().st_size==desc['bytes'] and path.stat().st_size<16*1024*1024 and file_sha(path)==desc['sha256'])
        values=json.loads(path.read_bytes())
        for item in values['blobs']:
            test('unique content address '+item['id'],item['id'] not in locations)
            locations[item['id']]=desc['path']
    test('unique blob count',len(locations)==index['blob_count'])
    files=index['files']
    test('all42 original names exactly',len(files)==42 and sorted(x['relative_path'] for x in files)==sorted(x.relative_to(original).as_posix() for x in original.rglob('*') if x.is_file()))
    raw_bytes=0
    referenced=set()
    zip_members=0
    for row in files:
        path=original/row['relative_path']
        test('exact original source identity '+row['relative_path'],path.stat().st_size==row['bytes'] and file_sha(path)==row['sha256'])
        expected_at=0;held=hashlib.sha256()
        with path.open('rb') as stream:
            for span in row['extents']:
                test('gapless ordered raw extent '+row['relative_path'],span['offset']==expected_at and span['bytes']>0)
                count=0;span_hash=hashlib.sha256()
                for ident in span['blob_ids']:
                    raw=blob(ident);referenced.add(ident)
                    observed=stream.read(len(raw))
                    test('direct exact reconstructed bytes '+row['relative_path'],raw==observed)
                    held.update(raw);span_hash.update(raw);count+=len(raw)
                test('raw span length and hash '+row['relative_path'],count==span['bytes'] and span_hash.hexdigest().upper()==span['sha256'])
                expected_at+=count
            test('raw reconstruction exact EOF '+row['relative_path'],expected_at==row['bytes'] and stream.read(1)==b'')
        test('whole reconstructed SHA '+row['relative_path'],held.hexdigest().upper()==row['sha256'])
        raw_bytes+=row['bytes']
        if row['zip'] is not None:
            info=row['zip'];zip_members+=len(info['members'])
            test('ZIP member count exact '+row['relative_path'],len(info['members'])==info['entries'])
            member_ids=set()
            with path.open('rb') as stream:
                for member in info['members']:
                    test('ZIP local index unique',member['index'] not in member_ids);member_ids.add(member['index'])
                    selected=[row['extents'][i] for i in member['extent_indexes']]
                    test('ZIP local membership exact',selected[0]['offset']==member['local_offset'] and sum(x['bytes'] for x in selected)==member['local_record_bytes'] and all(x['member']==member['index'] for x in selected))
                    stream.seek(member['local_offset']);header=stream.read(30)
                    fields=struct.unpack('<4s5H3I2H',header)
                    test('ZIP local raw metadata',fields[0]==b'PK\x03\x04' and fields[2]==member['flags'] and fields[3]==member['method'] and fields[6]==member['crc32'] and fields[7]==member['compressed_bytes'] and fields[8]==member['uncompressed_bytes'])
                    name=stream.read(fields[9])
                    test('ZIP local raw member name',name.hex().upper()==member['name_hex'])
                    stream.seek(member['local_offset']);local_hash=hashlib.sha256();remaining=member['local_record_bytes']
                    while remaining:
                        data=stream.read(min(remaining,8*1024*1024));test('ZIP local record bounds',len(data)==min(remaining,8*1024*1024));local_hash.update(data);remaining-=len(data)
                    test('ZIP complete local record raw SHA',local_hash.hexdigest().upper()==member['local_record_sha256'])
    test('all blobs referenced; nothing lost',referenced==set(locations))
    test('complete540637273-byte originals',raw_bytes==index['original_total_bytes']==540637273)
    test('deduplication algebra',index['extent_bytes_before_deduplication']==raw_bytes and index['deduplicated_raw_extent_bytes']==raw_bytes-index['unique_raw_extent_bytes'] and index['deduplicated_raw_extent_bytes']>0)
    meta=index['metadata_input_identities'];metadata_path=ROOT/meta['path']
    test('metadata companion exact identity',metadata_path.stat().st_size==meta['bytes'] and file_sha(metadata_path)==meta['sha256'])
    metadata=json.loads(metadata_path.read_bytes())
    pins=json.loads((original/'source-pins.json').read_bytes())
    test('all1381 metadata input identities preserved',metadata['rows']==pins['complete_inputs'] and metadata['row_count']==len(metadata['rows'])==1381 and metadata['original_source_pins_sha256']==file_sha(original/'source-pins.json'))
    test('no delivered file reaches16MiB',all(p.stat().st_size<16*1024*1024 for p in OUT.rglob('*') if p.is_file()))
    result={'status':'COMPLETE_LOSSLESS_RAW_BYTE_DELIVERY','original_files':len(files),'original_bytes':raw_bytes,'zip_local_records':zip_members,'blob_chunks':len(index['blob_chunks']),'unique_blobs':len(locations),'deduplicated_raw_extent_bytes':index['deduplicated_raw_extent_bytes'],'package_bytes':sum(p.stat().st_size for p in package.rglob('*') if p.is_file()),'metadata_input_rows':1381,'target_execution':False,'zip_extraction':False}
except BaseException:
    code=1;(ATT/'stderr.txt').write_text(traceback.format_exc(),encoding='utf-8',newline='\n')
    result={'status':'FAILED','source_only':True,'target_execution':False}
result.update({'checks':len(checks),'failed':sum(not x['pass'] for x in checks),'exit_code':code})
dump(ATT/'result.json',result)
dump(ATT/'checks.json',{'checks':checks,'result':result})
(ATT/'stdout.txt').write_text(json.dumps(result)+'\n',encoding='utf-8',newline='\n')
if not code:
    dump(OUT/'verification.json',result)
    freeze_rows=[rec(p) for p in sorted(OUT.rglob('*')) if p.is_file()]
    dump(FIRST,{'status':'FIRST_COMPLETE_WRITER_AND_INDEPENDENT_RECONSTRUCTION_GREEN_FROZEN','original_firstgreen':index['original_firstgreen_sha256'],'files':freeze_rows,'checks':result['checks'],'failed':0,'source_only':True,'target_execution':False,'zip_extraction':False})
print(json.dumps(result))
raise SystemExit(code)
