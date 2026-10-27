import json, os, pathlib, subprocess, tempfile, unittest, urllib.parse, fcntl, sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from make_duml import packet,crc

BIN = str(pathlib.Path(__file__).with_name('rtmp-test'))

class CGI(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        (self.root/'token').write_text('a'*64)
    def tearDown(self): self.tmp.cleanup()
    def call(self, action=None, **fields):
        env = dict(os.environ, OA_TEST_ROOT=str(self.root), REQUEST_METHOD='GET', HTTP_HOST='192.168.100.117')
        body = ''
        if action:
            body = urllib.parse.urlencode(dict(action=action, token='a'*64, **fields))
            env.update(REQUEST_METHOD='POST', CONTENT_TYPE='application/x-www-form-urlencoded', CONTENT_LENGTH=str(len(body)), HTTP_ORIGIN='http://192.168.100.117',HTTP_REFERER='http://192.168.100.117/')
        p = subprocess.run([BIN], input=body, text=True, capture_output=True, env=env, timeout=5)
        return json.loads(p.stdout.split('\n\n',1)[1])
    def test_config_roundtrip(self):
        self.assertEqual(self.call()['bitrate'],2000)
        self.assertTrue(self.call('save',url='rtmp://192.168.100.6:1935/live/test?key=a&x=b',bitrate='2500')['ok'])
        self.assertEqual(self.call()['bitrate'],2500)
    def test_invalid_inputs_do_not_save(self):
        for url,rate in [('http://host/a','2000'),('rtmp://host/\nfoo','2000'),('rtmp://host/a','499'),('rtmp://host/a','6001'),('rtmp://host/a','2000x')]:
            self.assertFalse(self.call('save',url=url,bitrate=rate)['ok'])
        self.assertFalse((self.root/'config').exists())
    def test_missing_bridge(self):
        r=self.call('status'); self.assertFalse(r['ok']); self.assertEqual(r['code'],'bridge_missing')
    def test_concurrent_request(self):
        with open(self.root/'lock','w') as f:
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
            self.assertEqual(self.call('status')['code'],'busy')
    def test_csrf(self):
        (self.root/'token').write_text('b'*64)
        self.assertEqual(self.call('save',url='rtmp://host/a',bitrate='2000')['code'],'forbidden')
    def test_protocol_fixture(self):
        result=subprocess.check_output([BIN,'--packet'],text=True).strip()
        self.assertEqual(result,packet(bytes.fromhex('00011c00'),0x4001).hex())
    def test_ignore_old_ack_and_accept_new_ack(self):
        def ack(payload):
            b=bytearray(packet(b'\0'+payload,0x4001)); b[4:6]=bytes([1,2]);b[8]=0x80
            b[-2:]=crc(b[:-2],0x3692,0x8408).to_bytes(2,'little')
            return '[123]:'+b.hex(' ')+' \n'
        old=ack(bytes.fromhex('00011c0009033001000000d00700'))
        new=ack(bytes.fromhex('00011c0009033002000000d00700'))
        log=self.root/'reply';log.write_text(old)
        args=[BIN,'--reply-fixture',str(log),str(len(old))]
        self.assertEqual(subprocess.check_output(args,text=True).strip(),'0:')
        log.write_text(old+new)
        self.assertEqual(subprocess.check_output(args,text=True).strip(),'14:00011c0009033002000000d00700')

if __name__=='__main__': unittest.main()
