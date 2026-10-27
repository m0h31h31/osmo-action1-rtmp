"""Host simulation of fixed-address loader, using a private sparse file.

Run under WSL Python. Never touches /dev/mem or a real camera.
"""
import json,mmap,re,struct,subprocess,tempfile,threading,time
from pathlib import Path
HERE=Path(__file__).resolve().parent
header=(HERE/'loader_payload.h').read_text()
arrays={name:bytes(map(int,body.split(','))) for name,body in re.findall(r'unsigned char (\w+)\[\] = \{([^}]+)\}',header)}
def block(name):return re.search(r'\b'+name+r'\[\] = \{(.*?)\n\};',header,re.S)[1]
patches=[tuple(int(x,16) for x in row) for row in re.findall(r'\{(0x\w+),(0x\w+),(0x\w+)\}',block('patches'))]
gates=[tuple(int(x,16) for x in row) for row in re.findall(r'\{(0x\w+),(0x\w+),(0x\w+)\}',block('uvc_gates'))]
signatures=[(int(a,16),[int(x,16) for x in words.split(',')]) for a,words in re.findall(r'\{(0x\w+),\{([^}]+)\}\}',block('signatures'))]
capture=int(re.search(r'#define CAPTURE_OFFSET (\d+)u',header)[1])
CTX=0x25ca890
with tempfile.TemporaryDirectory(prefix='oa_snake_loader_') as temp:
    root=Path(temp)
    for folder in ['dev','proc','tmp/user_data','tmp/SD0']:(root/folder).mkdir(parents=True,exist_ok=True)
    binary=root/'loader'
    subprocess.run(['clang-18','-std=c99','-Wall','-Wextra','-Werror','-O2','-DSNAKE_ROOT='+json.dumps(temp),str(HERE/'loader.c'),'-o',str(binary)],check=True)
    (root/'proc/modules').write_text('')  # Formal firmware does not enable USB Ethernet
    with (root/'dev/mem').open('w+b') as file:
        file.truncate(0x3000000)
        memory=mmap.mmap(file.fileno(),0)
        def put(a,*values):memory[a:a+4*len(values)]=struct.pack('<%dI'%len(values),*values)
        def get(a):return struct.unpack('<I',memory[a:a+4])[0]
        def reset():
            memory[0x843b94:0x843b94+len(arrays['code_old'])]=arrays['code_old']
            for a,old,new in patches+gates:put(a,old)
            for a,words in signatures:put(a,*words)
            put(0x25ca628,0xb25a2e3c,0xb25a2e3c)
            memory[CTX:CTX+0x510]=struct.pack('<I',0xa90f36c5)*(0x510//4)
            put(0x25cadb0,0xb25a2e3c)
            (root/'tmp/user_data/oa_snake_failed').unlink(missing_ok=True)
            (root/'tmp/SD0/OA_SNAKE_DISABLE').unlink(missing_ok=True)
        def run(*args,beat=False,held=False):
            halt=threading.Event()
            def pump():
                tick=1000
                while not halt.wait(.05):
                    if get(CTX)==0x534e4b45:
                        tick+=50;put(CTX+16,tick)
                        if held:memory[CTX+capture:CTX+capture+2]=b'\x01\x00'
            thread=threading.Thread(target=pump)
            if beat:thread.start()
            try:return subprocess.run([str(binary),*args],capture_output=True,text=True,timeout=15)
            finally:
                halt.set()
                if beat:thread.join()
        reset();before=bytes(memory[0x843b94:0x843b94+len(arrays['code_old'])])
        assert run('--check').returncode==0
        assert bytes(memory[0x843b94:0x843b94+len(before)])==before
        put(patches[0][0],0xdeadbeef);assert run('--check').returncode==1
        assert get(patches[0][0])==0xdeadbeef
        reset();put(0x25ca628,0);assert run('--check').returncode==1
        reset();(root/'tmp/user_data/oa_snake_failed').write_text('prior failure')
        assert run().returncode==1 and get(patches[0][0])==patches[0][1]
        reset();(root/'tmp/SD0/OA_SNAKE_DISABLE').touch()
        assert run().returncode==0 and get(patches[0][0])==patches[0][1]
        reset();result=run('--trial','3',beat=True);assert result.returncode==0,result.stderr
        assert all(get(a)==old for a,old,new in patches)
        assert all(get(a)==new for a,old,new in gates)
        assert bytes(memory[0x843b94:0x843b94+len(arrays['code_new'])])==arrays['code_new']
        assert get(CTX+8)==0 and get(CTX+12)==0xffffffff
        assert not (root/'tmp/user_data/oa_snake_failed').exists()
        reset();result=run('--trial','3',beat=True,held=True);assert result.returncode==0,result.stderr
        for a,old,new in patches:assert get(a)==(new if a in [0x8eca20,0x8eca24] else old)
        assert get(CTX+8)==0
        reset();result=run('--trial','3');assert result.returncode==1,result.stderr
        assert all(get(a)==old for a,old,new in patches)
        assert (root/'tmp/user_data/oa_snake_failed').exists()
        memory.close()
print('PASS: loader preflight, mismatches, prior failure, disable flag, heartbeat, retained code, captured-tail shutdown, watchdog')
