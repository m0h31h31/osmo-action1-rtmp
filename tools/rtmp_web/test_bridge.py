import pathlib,subprocess,sys,unittest,struct
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from make_duml import packet
BIN=str(pathlib.Path(__file__).with_name('bridge-test'))
class Bridge(unittest.TestCase):
    def check(self,raw): return subprocess.check_output([BIN],input=raw).strip()==b'valid'
    def test_verified_requests(self):
        for p in ['00011c00','01011a000100','01011a000101']:
            self.assertTrue(self.check(packet(bytes.fromhex(p))))
        url=b'rtmp://192.168.100.6:1935/live/osmo\0'
        body=bytes([0])+struct.pack('<H',2000)+bytes(6)+struct.pack('<H',len(url))+url
        self.assertTrue(self.check(packet(bytes([1,1,25,0,len(body)])+body)))
    def test_rejects_corruption_and_unrelated_commands(self):
        raw=bytearray(packet(bytes.fromhex('00011c00')));raw[-1]^=1
        self.assertFalse(self.check(raw))
        self.assertFalse(self.check(packet(bytes.fromhex('01011a000102'))))
        self.assertFalse(self.check(packet(bytes.fromhex('00011b00'))))
        self.assertFalse(self.check(b''))
if __name__=='__main__':unittest.main()
