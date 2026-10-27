"""Bind loader to exact source firmware and freshly linked payload."""
import hashlib,struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
HERE=Path(__file__).parent
image=(HERE.parents[1]/'analysis_bins/rtos_decompressed.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='662bc2f4f8700bb0edd82af26c93d8efd94a75045e632b12ce635920648bfa92'
code=(HERE/'payload.bin').read_bytes();labels=(HERE/'payload.labels.bin').read_bytes()
slot=0x843b94
assert len(code)<=0x1698 and 576+len(labels)<=0x4c0
code+=image[slot-0x20000+len(code):slot-0x20000+((len(code)+3)&~3)]
labels+=bytes((-len(labels))%4)
with (HERE/'payload.elf').open('rb') as f:
    elf=ELFFile(f);s={x.name:x['st_value'] for x in elf.get_section_by_name('.symtab').iter_symbols()}
    section=elf.get_section_by_name('.rodata');offset=s['oa_layout']-section['sh_addr']
    context_size,capture_offset,entry_offset=struct.unpack_from('<3I',section.data(),offset)
    assert context_size==576
patches=[(0x8ec664,s['oa_draw']),(0x8eca20,s['oa_move']),(0x8eca24,s['oa_hit']),
         (0x27d830,0xeb000000|(((s['oa_timers']-0x27d838)//4)&0xffffff))]
out=['/* Generated; do not hand-edit. */','#define CODE_ADDRESS 0x843b94u','#define DATA_ADDRESS 0x25ca890u','#define LABEL_ADDRESS 0x25caad0u','#define CONTEXT_BYTES 576u','#define MAGIC 0x534e4b45u']
out += ['#define CAPTURE_OFFSET %du'%capture_offset,'#define ENTRY_OFFSET %du'%entry_offset]
def array(name,data):
    out.append('static const unsigned char %s[] = {%s};'%(name,','.join(map(str,data))))
array('code_new',code);array('code_old',image[slot-0x20000:slot-0x20000+len(code)]);array('labels',labels)
out.append('static const struct Patch patches[] = {')
for a,new in patches:
    old=struct.unpack_from('<I',image,a-0x20000)[0]
    out.append('{0x%x,0x%x,0x%x},'%(a,old,new))
out.append('};')
# Disable the sole sample initialization entry points while their static
# memory/code is reserved. Never touch the generic USB / file-transfer APIs.
out.append('static const struct Patch uvc_gates[] = {')
for a,new in [(0x84522c,0xe3a00000),(0x845230,0xe12fff1e),(0x845304,0xe3e00000),(0x845308,0xe12fff1e)]:
    out.append('{0x%x,0x%x,0x%x},'%(a,struct.unpack_from('<I',image,a-0x20000)[0],new))
out.append('};')
addresses=[0x2601b8,0x2134b0,0x25fe2c,0x112fd4,0x12302c,0x984a78,0x8ec63c,0x8ec988]
out.append('static const struct Signature signatures[] = {')
for a in addresses:
    out.append('{0x%x,{%s}},'%(a,','.join('0x%x'%x for x in struct.unpack_from('<4I',image,a-0x20000))))
out.append('};')
(HERE/'loader_payload.h').write_text('\n'.join(out)+'\n',encoding='ascii')
print('Loader header:',len(code),'code bytes;',len(labels),'label bytes; exact source signatures pinned')
