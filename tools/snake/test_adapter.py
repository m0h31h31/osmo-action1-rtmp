"""Execute linked ARM adapter offline with native services intercepted.

This validates integration logic, NOT ownership/permissions of candidate RTOS RAM.
"""
import struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.arm_const import *

HERE=Path(__file__).parent
with (HERE/'payload.elf').open('rb') as file:
    elf=ELFFile(file)
    symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
u.mem_map(0x20000,0x900000)
u.mem_write(0x843b94,(HERE/'payload.bin').read_bytes())
u.mem_protect(0x20000,0x900000,UC_PROT_READ|UC_PROT_EXEC)
u.mem_map(0x25ca000,0x2000)
u.mem_write(0x25caad0,(HERE/'payload.labels.bin').read_bytes())
u.mem_map(0xc000000,0x10000)
u.mem_map(0x10000000,0x10000)
ctx,root,page=0x25ca890,0xc000000,0xc001000
stack,stop=0x1000f000,0x20000
clock=[1000]; busy=[0]; paints=[]; native=[]
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]
def put(a,*v):u.mem_write(a,struct.pack('<%dI'%len(v),*v))
put(ctx,0x534e4b45,1,1,1000000)
put(root,0x8ec988); put(root+0x3c,page)
put(page,0x8ec63c);put(page+0x18,root);put(page+0x1c,0x7f)
def hook(uc,addr,size,_):
    if addr not in [0x12302c,0x116a44,0x25fe2c,0x112fd4,0x161b00,0x15e8b8,0x264848,0x2601b8,0x2134b0]: return
    args=[uc.reg_read(r) for r in regs]
    if addr==0x12302c:
        args+=list(struct.unpack('<10I',uc.mem_read(uc.reg_read(UC_ARM_REG_SP),40)))
        paints.append(args)
    else:native.append(addr)
    result=clock[0] if addr==0x25fe2c else 0
    if addr==0x2601b8:
        assert args[:2]==[0x9824ac,0x984a78]
        result=0xc008000
    if addr==0x2134b0:
        assert args[0]==0xc008000
        result=busy[0]
    uc.reg_write(UC_ARM_REG_R0,result)
    uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
u.hook_add(UC_HOOK_CODE,hook)
def call(name,*args):
    for reg,val in zip(regs,list(args)+[0]*4):u.reg_write(reg,val)
    if len(args)>4:put(stack,*args[4:])
    u.reg_write(UC_ARM_REG_SP,stack);u.reg_write(UC_ARM_REG_LR,stop)
    u.emu_start(symbols[name],stop,count=2000000)
    assert u.reg_read(UC_ARM_REG_PC)==stop,name
    assert u.reg_read(UC_ARM_REG_SP)==stack,name
    return u.reg_read(UC_ARM_REG_R0)
def draw():
    paints.clear();call('oa_draw',page,0x123456,0,0,640,360,0,0,255,1)
def hit(down,x,y,id=0):return call('oa_hit',root,down,id,x,y)
def head():return [p[5:9] for p in paints if p[9]==0xff99ef68]
draw();assert head()==[] and 0x116a44 in native
# An unrelated menu touch is forwarded exactly once.
native.clear();assert hit(1,10,200)==0;assert native.count(0x161b00)==1
hit(0,10,200)
# A native-owned contact must finish before a different finger can enter.
hit(1,10,200,0);hit(1,480,24,1)
native.clear();hit(0,10,200,0);assert native.count(0x161b00)==1
hit(0,480,24,1);draw();assert not head()
# Multi-finger entry is cancelled; all released tails are swallowed.
assert hit(1,480,24)==1
assert hit(1,100,100,1)==1
assert hit(0,480,24)==1
native.clear();assert hit(0,100,100,1)==1;assert 0x161b00 not in native
draw();assert head()==[]
assert hit(1,480,24)==1;assert hit(0,480,24)==1
draw();assert len(head())==1
before=head()[0]
assert hit(1,320,344)==1;assert hit(0,320,344)==1
clock[0]+=200;call('oa_timers');draw();after=head()[0]
assert after[0]==before[0]+24
hit(1,320,180);assert call('oa_move',root,0,320,140)==1;hit(0,320,140)
clock[0]+=200;call('oa_timers');draw();assert head()[0][1]==after[1]-24
# Exit restores native rendering and swallows the exit finger release.
hit(1,600,16);native.clear();assert hit(0,600,16)==1
assert 0x161b00 not in native
native.clear();draw();assert not head() and 0x116a44 in native
# Native capture/live-show busy status cancels play and blocks entry.
hit(1,480,24);hit(0,480,24);draw();assert head()
busy[0]=1;call('oa_timers');draw();assert not head()
hit(1,480,24);hit(0,480,24);draw();assert not head()
busy[0]=0
# Deadline prevents new entry and expires an active game.
hit(1,480,24);hit(0,480,24);clock[0]=1000001;call('oa_timers')
draw();assert not head()
# A delayed/suspended Linux loader must not evict a native game. Production
# uses an unbounded native lifetime, not a five-second renewed lease.
put(ctx+8,1,0xffffffff)
hit(1,480,24);hit(0,480,24);draw();assert head()
hit(1,320,344);hit(0,320,344)
clock[0]+=65000;call('oa_timers');draw();assert head()
busy[0]=1;call('oa_timers');draw();assert not head()
busy[0]=0
assert call('oa_context_size')<=0x510
# Stock UI may keep an empty message wrapper above Settings after a dialog.
# Empty wrapper is transparent; a populated wrapper must still block/preempt.
wrapper=0xc009000
put(wrapper,0x976bb8);put(wrapper+0x14,page);put(wrapper+0x1c,0xf)
put(root+0x3c,wrapper)
hit(1,480,24);hit(0,480,24);draw();assert head(), 'empty native wrapper blocked entry'
put(wrapper+0x38,0xc009800);put(wrapper+0x3c,0xc009800)
call('oa_timers');draw();assert not head(), 'real dialog failed to preempt'
hit(1,480,24);hit(0,480,24);draw();assert not head()
put(wrapper+0x38,0);put(wrapper+0x3c,0)
hit(1,480,24);hit(0,480,24);draw();assert head()
hit(1,600,16);hit(0,600,16)
for numerator,denominator in [(0,1),(1,20),(239,20),(0xffffffff,1),(0xffffffff,20),(0xffffffff,0x80000000),(0x80000000,3)]:
    quotient=call('__aeabi_uidivmod',numerator,denominator)
    assert quotient==numerator//denominator
    assert u.reg_read(UC_ARM_REG_R1)==numerator%denominator
print('PASS: ARM menu entry, multitouch tails, native forwarding, start/tick/swipe, exit, timeout; RX payload')
