"""Offline CGI and card parser tests; compile/run inside WSL."""
import json, os, subprocess, tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='oa_network_') as d:
 root=Path(d)
 for folder in ['tmp/SD0','tmp/oa_rtmp_web','proc']:(root/folder).mkdir(parents=True)
 (root/'proc/mounts').write_text('e: /tmp/SD0 ambafs rw 0 0\n')
 token='a'*64;(root/'tmp/oa_rtmp_web/token').write_text(token)
 (root/'tmp/oa_network.mode').write_text('sta\n')
 binary=root/'network'
 subprocess.run(['clang-18','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DNET_ROOT='+json.dumps(d),str(HERE/'network.c'),'-o',str(binary)],check=True)
 conf=root/'tmp/SD0/wifi.conf';output=root/'wpa'
 def prepare(text):
  conf.write_text(text);output.unlink(missing_ok=True)
  return subprocess.run([str(binary),'--prepare-config',str(conf),str(output)]).returncode
 assert prepare('WIFI_MODE=ap\nESSID="Test space $()"\nPASSWORD=\'a"b\\c123\'\n')==0
 data=output.read_text();assert 'ssid='+b'Test space $()'.hex() in data and 'psk="a\\"b\\\\c123"' in data
 assert prepare('ESSID=test\nPASSWORD=$(touch /tmp/SHOULD_NOT_EXIST)\n')==0
 assert not Path('/tmp/SHOULD_NOT_EXIST').exists()
 for text in ['ESSID=test\nPASSWORD=short\n','ESSID=test\nESSID=duplicate\nPASSWORD=12345678\n','ESSID=test\nPASSWORD="unterminated\n','PASSWORD=12345678\n','ESSID=test\nPASSWORD='+'x'*64+'\n']:
  assert prepare(text)==3 and not output.exists()
 assert prepare('ESSID=test\nPASSWORD=\n')==0
 assert prepare('ESSID=test\r\nPASSWORD=12345678\r\n')==0
 def api(method='GET',body='',**overrides):
  env=dict(os.environ,REQUEST_METHOD=method,CONTENT_TYPE='application/x-www-form-urlencoded',HTTP_HOST='camera',HTTP_REFERER='http://camera/',CONTENT_LENGTH=str(len(body)))
  env.update(overrides)
  p=subprocess.run([str(binary)],input=body,text=True,capture_output=True,env=env,check=True)
  return json.loads(p.stdout.split('\n\n',1)[1])
 assert api()['enabled'] and not api()['pending']
 body='action=disable&token='+token
 assert api('POST',body,HTTP_REFERER='http://camera.evil/')['code']=='forbidden'
 assert not (root/'tmp/SD0/OA_STA_DISABLE').exists()
 r=api('POST',body);assert not r['enabled'] and r['pending'] and r['active']=='sta'
 assert conf.read_text()=='ESSID=test\nPASSWORD=12345678\n'
 assert api('POST','action=enable&token='+token)['enabled']
 (root/'tmp/SD0/NO_WIFI').touch();assert not api()['enabled']
 assert api('POST','action=enable&token='+token)['code']=='legacy_disabled'
 (root/'tmp/SD0/NO_WIFI').unlink()
 conf.unlink();assert api('POST','action=enable&token='+token)['code']=='config_invalid'
 (root/'proc/mounts').write_text('');assert api('POST',body)['code']=='card_missing'
 assert api('PUT')['code']=='method'
print('PASS: literal credentials, escaping, invalid config, no execution, POST/CSRF, persistence, pending state, missing card')
