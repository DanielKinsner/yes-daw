import hashlib,json,pathlib,re,struct
BASE=pathlib.Path(__file__).resolve().parent.parent
ROOT=BASE.parent.parent
folder=ROOT/'build-ci/20261105-musical-funded-r5-force-emission-object-r1/attempt-r1'
obj=(folder/'app-compute.obj').read_bytes()
listing=(folder/'app-compute-disassembly.txt').read_text().splitlines()
assert obj[:4]==b'\0\0\xff\xff'
nsec,ptr,nsyms=struct.unpack_from('<III',obj,44)
strtab=ptr+nsyms*20
def name(b):
 if b[:4]==b'\0'*4:
  off=struct.unpack('<I',b[4:])[0]
  return obj[strtab+off:obj.index(0,strtab+off)].decode(errors='replace')
 return b.split(b'\0')[0].decode(errors='replace')
sections={}
for i in range(1,nsec+1):
 off=56+(i-1)*40
 nm=obj[off:off+8].split(b'\0')[0].decode()
 size,raw,reloc=struct.unpack_from('<III',obj,off+16)
 count=struct.unpack_from('<H',obj,off+32)[0]
 sections[i]={'name':nm,'bytes':size,'raw_offset':raw,'reloc_offset':reloc,'reloc_count':count,'sha256':hashlib.sha256(obj[raw:raw+size]).hexdigest().upper()}
symbols={};i=0
while i<nsyms:
 off=ptr+i*20
 nm=name(obj[off:off+8]);val,sec,ty,sc,aux=struct.unpack_from('<IiHBB',obj,off+8)
 symbols[i]={'name':nm,'value':val,'section':sec,'type':ty,'storage':sc,'aux':aux}
 i+=1+aux
markers={int(m.group(1),16):i for i,s in enumerate(listing) if (m:=re.match(r'RELOCATIONS #([0-9A-F]+)$',s))}
targets={
 'old-append':'?appendTempoSegmentFrames@detail@engine',
 'compiled-build':'?build@CompiledTempoMap@',
 'compiled-tick':'?frameForTick@CompiledTempoMap@',
 'compiled-musical-time':'?frameForMusicalTime@CompiledTempoMap@',
 'clock-build':'?build@MusicalAudioClock@',
 'clock-frameAt':'?frameAt@MusicalAudioClock@',
 'classify':'?classifyAppend@FundedWindowQuery@',
 'prefix':'?prefixTo@FundedWindowQuery@',
 'append-new':'?append@FundedWindowQuery@',
 'endpoint':'?endpoint@FundedWindowQuery@',
 'finish-endpoint':'?finishEndpoint@FundedWindowQuery@',
 'predict':'?predict@FundedWindowQuery@',
 'resolve-new':'?resolve@FundedWindowQuery@',
 'resolve-old':'?resolveMusicalAudioWindow@',
 'native-restore':'?restore@FundedNativeSession@',
 'worker-wrapper':'?emitWorker@r5_codegen_evidence@',
}
out={'schema':'FILE_ONLY_EXISTING_COFF_AND_DUMP_SECTION_ASSOCIATION','scope':'Ordinary binary metadata and existing decoded-listing reads only; no compiler/disassembler/target execution','object_sha256':hashlib.sha256(obj).hexdigest().upper(),'listing_sha256':hashlib.sha256((folder/'app-compute-disassembly.txt').read_bytes()).hexdigest().upper(),'coff_sections':nsec,'symbol_records_including_aux':nsyms,'targets':{}}
for label,prefix in targets.items():
 found=[(idx,s) for idx,s in symbols.items() if s['name'].startswith(prefix) and s['section']>0]
 if len(found)!=1:
  out['targets'][label]={'matches':found,'status':'NEEDS_EXACT_BODY_ASSOCIATION'};continue
 idx,sym=found[0];sec=sym['section'];r=sections[sec]
 if sec not in markers:
  out['targets'][label]={'symbol':sym,'section':r,'status':'NO_RELOCATION_MARKER_NEEDS_DIRECT_ASSOCIATION'};continue
 end=markers[sec]
 start=max(i for i in range(end) if re.match(r'  0000000000000000:',listing[i]))
 reloc_end=end+1
 while reloc_end<len(listing) and (not re.match(r'  0000000000000000:',listing[reloc_end])) and not listing[reloc_end].startswith('COFF SYMBOL TABLE'):
  reloc_end+=1
 lines=listing[start:reloc_end]
 # Reassemble only bytes already displayed by root's decoder, not a new decoder.
 shown={};cursor=0
 for line in listing[start:end]:
  m=re.match(r'  ([0-9A-F]{16}): ((?:[0-9A-F]{2} )+)',line)
  if m:cursor=int(m[1],16);bs=bytes.fromhex(m[2])
  else:
   m=re.match(r'                    ((?:[0-9A-F]{2} ?)+)$',line)
   if not m:continue
   bs=bytes.fromhex(m[1])
  for b in bs:shown[cursor]=b;cursor+=1
 raw=obj[r['raw_offset']:r['raw_offset']+r['bytes']]
 complete=len(shown)==len(raw) and all(shown.get(i)==b for i,b in enumerate(raw))
 calls=[x.strip() for x in listing[start:end] if re.search(r'\b(call|jmp)\s+.*(?:\?|__imp_)',x)]
 fp=[x.strip() for x in listing[start:end] if re.search(r'\b(?:v?fm\w+|v?fnm\w+|f(?:add|sub|mul|div|ld|st|sin|cos|yl2x)\w*|ldmxcsr|stmxcsr|fldcw|fnstcw)\b',x,re.I)]
 rec={'symbol_index':idx,'symbol':sym['name'],'section_number':sec,'section':r,'listing_lines_1based':[start+1,reloc_end],'displayed_bytes_match_entire_raw_section':complete,'direct_named_calls_or_jumps':calls,'x87_FMA_or_control_instruction_lines':fp,'mnemonics':{}}
 for line in listing[start:end]:
  m=re.match(r'  [0-9A-F]{16}: (?:[0-9A-F]{2} )+\s+([a-z][a-z0-9]+)\s',line)
  if m:rec['mnemonics'][m[1]]=rec['mnemonics'].get(m[1],0)+1
 (BASE/'evidence'/f'{label}.txt').write_bytes((f'# {label}; COFF section{sec:X}; source listing lines{start+1}..{reloc_end}\n# {sym["name"]}\n'+'\n'.join(lines)+'\n').encode())
 out['targets'][label]=rec
(BASE/'object-sections.json').write_bytes((json.dumps(out,indent=2)+'\n').encode())
print(json.dumps({k:{'section':v.get('section_number'),'bytes':v.get('section',{}).get('bytes'),'display_bytes_match':v.get('displayed_bytes_match_entire_raw_section'),'special_fp_count':len(v.get('x87_FMA_or_control_instruction_lines',[])),'status':v.get('status','EXTRACTED')} for k,v in out['targets'].items()},indent=2))
