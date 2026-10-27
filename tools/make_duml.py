import struct,sys
from pathlib import Path
def crc(data,seed,poly):
 for v in data:
  seed^=v
  for _ in range(8):seed=(seed>>1)^poly if seed&1 else seed>>1
 return seed
def packet(payload,seq=0x4001):
 p=bytearray(b'\x55'+struct.pack('<H',len(payload)+13|0x400))
 p.append(crc(p,0x77,0x8c));p+=bytes([2,1])+struct.pack('<H',seq)+bytes([0x40,2,0x8e])+payload
 return p+struct.pack('<H',crc(p,0x3692,0x8408))
if __name__=='__main__':
 data=packet(bytes.fromhex(sys.argv[1]));Path('analysis_bins/duml.req').write_bytes(data);print(data.hex())
