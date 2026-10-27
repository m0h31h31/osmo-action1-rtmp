"""Run boot ownership scripts against isolated fake paths and fake hardware."""
import os,subprocess,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='oa_boot_') as d:
 root=Path(d)
 for folder in ['tmp/SD0','tmp/user_data','proc','sys/module/bcmdhd','sys/class/net/wlan0','usr/bin','usr/local/share/script','usr/lib/oa-stock','bin']:(root/folder).mkdir(parents=True,exist_ok=True)
 (root/'proc/mounts').write_text('e: /tmp/SD0 ambafs rw 0 0\n')
 def script(path,text):path.write_text(text);path.chmod(0o755)
 for name in ['sleep','sync','ifconfig']:script(root/'bin'/name,'#!/bin/sh\nexit 0\n')
 script(root/'usr/bin/oa_network_api','#!/bin/sh\n[ -f "$2" ] && cp "$2" "$3"\n')
 script(root/'usr/local/share/script/sta_start.dji.sh',f'#!/bin/sh\necho "$@" >> {root}/sta.calls\n')
 script(root/'usr/lib/oa-stock/dji_wms_ac101',f'#!/bin/sh\necho native >> {root}/wms.calls\n')
 def rewrite(name):
  data=(HERE/name).read_text()
  # Absolute firmware paths only; keep mount-table matching as /tmp/SD0.
  data=data.replace('/tmp/','@TMP@').replace('/proc/','@PROC@').replace('/sys/','@SYS@').replace('/usr/','@USR@')
  for key,sub in [('TMP','tmp'),('PROC','proc'),('SYS','sys'),('USR','usr')]:data=data.replace('@'+key+'@',str(root/sub)+'/')
  data=data.replace("' "+str(root)+"/tmp/SD0 '","' /tmp/SD0 '")
  target=root/name;script(target,data);return target
 network=rewrite('oa_network');wms=rewrite('dji_wms_ac101')
 env=dict(os.environ,PATH=str(root/'bin')+':'+os.environ['PATH'])
 def run(path,*args):return subprocess.run([str(path),*args],env=env,capture_output=True,text=True,timeout=5)
 assert run(network,'prepare').returncode==0
 assert (root/'tmp/oa_network.mode').read_text()=='sta\n'
 assert run(wms).returncode==0 and not(root/'wms.calls').exists()
 assert run(network,'start').returncode==1 and not(root/'sta.calls').exists()
 (root/'tmp/SD0/wifi.conf').write_text('fixture')
 assert run(network,'start').returncode==0
 assert (root/'sta.calls').read_text()=='prepared\n'
 mac=(root/'tmp/user_data/oa_sta_mac').read_text();assert mac.startswith('02:') and len(mac.strip())==17
 assert run(network,'start').returncode==0 and (root/'tmp/user_data/oa_sta_mac').read_text()==mac
 for flag in ['OA_STA_DISABLE','NO_WIFI']:
  (root/'tmp/SD0'/flag).touch();assert run(network,'prepare').returncode==0
  assert (root/'tmp/oa_network.mode').read_text()=='system\n';assert run(wms).returncode==0
  before=(root/'sta.calls').read_text();assert run(network,'start').returncode==0;assert (root/'sta.calls').read_text()==before
  (root/'tmp/SD0'/flag).unlink()
 assert (root/'wms.calls').read_text()=='native\nnative\n'
 (root/'proc/mounts').write_text('');assert run(network,'prepare').returncode==0
 assert (root/'tmp/oa_network.mode').read_text()=='system\n'
print('PASS: exclusive boot ownership, no custom AP fallback, card flags, native delegation, stable per-device MAC')
