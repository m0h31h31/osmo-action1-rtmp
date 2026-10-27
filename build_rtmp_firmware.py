"""Build v17 persistent web/RTMP firmware; never transfers to or flashes a camera."""
from pathlib import Path
import sys,struct,hashlib,zlib,json,subprocess,shutil,zipfile,argparse
from ctypes import sizeof,c_ubyte
import lz4.frame
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools'))
import dji_xv4_fwcon as X
import amba_fwpak as A
parser=argparse.ArgumentParser();parser.add_argument('--revision',type=int,choices=[17,18,19,20,21],default=21);args=parser.parse_args()
REV=args.revision;PATCH=REV+62;MODULE=REV+7290
WORK=ROOT/f'firmware/build_v{REV}'
OUT=ROOT/f'firmware/flash_root_v{REV}'
WEB=ROOT/'tools/rtmp_web'
WORK.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
def linux(p):
    p=Path(p).resolve();return '/mnt/'+p.drive[0].lower()+p.as_posix()[2:]
def run(*args,allowed=(0,)):
    p=subprocess.run(['wsl','-d','Ubuntu-24.04','--',*map(str,args)],capture_output=True,text=True)
    if p.returncode not in allowed:raise RuntimeError(p.stdout+p.stderr)
    return p.stdout+p.stderr
src=(ROOT/'analysis_bins/v15.bin').read_bytes()
assert hashlib.sha256(src).hexdigest()=='1d019cc373ed0ddc5f033b0a6f3c1c4cac81be5dc9091c15ca3ebcc7dd9deaab'
h=X.FwPkgHeader.from_buffer_copy(src);base=sizeof(X.FwPkgHeader);e=X.FwPkgEntry.from_buffer_copy(src[base:])
raw=lz4.frame.decompress(src[e.dt_offs:e.dt_offs+e.stored_len]);parts=[];pos=0x204
for i in range(5):
    n,cum=struct.unpack_from('<II',raw,44+8*i);parts.append((pos,n,raw[pos:pos+n]));pos+=n
assert pos==len(raw)
pos,length,oldpart=parts[4];image=WORK/f'rootfs_v{REV}.img';image.write_bytes(oldpart[256:])
assert image.read_bytes()==(ROOT/'analysis_bins/rootfs_v15.img').read_bytes()
def extract(path):
    output=WORK/('original_'+Path(path).name)
    run('debugfs','-R',f'dump {path} {linux(output)}',linux(image))
    return output.read_bytes()
startup=extract('/etc/init.d/S50service_dji').decode()
assert startup.count('# dji_wms_ac101 &  (disabled by mod)')==1
startup=startup.replace('# dji_wms_ac101 &  (disabled by mod)', '/usr/bin/dji_wms_ac101 &' if REV>=20 else ': # WMS intentionally disabled')
assert startup.count('/usr/bin/cmd_rlv_transfer &')==1
startup=startup.replace('/usr/bin/cmd_rlv_transfer &','/usr/bin/oa_cmd_start &')
(WORK/'S50service_dji').write_text(startup,encoding='utf-8',newline='\n')
rootshell=extract('/etc/init.d/S55rootshell').decode()
assert rootshell.count('patch v15 / fw 01.10.77 / module 01.81.7305')==1
rootshell=rootshell.replace('patch v15 / fw 01.10.77 / module 01.81.7305',f'patch v{REV} / fw 01.10.{PATCH} / module 01.81.{MODULE}')
(WORK/'S55rootshell').write_text(rootshell,encoding='utf-8',newline='\n')
original=extract('/usr/local/www/index.html').decode('utf-8')
panel=(WEB/'panel.html').read_text(encoding='utf-8')
panel=panel.replace('相机重启后需恢复临时控制服务和网页部署；保存配置不会自动开播。','重启后网页与控制服务自动就绪；不会自动开播，请手动点击“启动推流”。')
if REV == 19:
    panel += '''
<section class="card"><h2>USB 数据线直播</h2>
<p>开机约 1 分钟后准备 USB 网络。电脑会识别为 USB 网卡。</p>
<p>USB 管理网页：<a href="http://192.168.77.2/">192.168.77.2</a></p>
<p>推流地址：<code>rtmp://192.168.77.1:1935/live/osmo</code></p>
<p>先运行电脑接收服务，在上方保存地址，再点击“启动推流”。开机不会自动推流。</p>
<p id="usb-state">正在读取 USB 状态…</p>
<p>USB 网卡模式下请勿选择文件传输。需要恢复原厂 USB 模式时，在存储卡根目录创建 OA_USB_DISABLE 空文件，再正常关机开机。</p></section>
<script>(()=>{async function update(){try{const s=await(await fetch('/cgi-bin/usb-status',{cache:'no-store'})).json();const names={ready:'USB 服务已准备',disabled:'USB 网卡已停用',loading:'正在加载',waiting_for_camera:'等待相机初始化',error:'启动失败，请检查日志',not_started:'尚未启动'};document.getElementById('usb-state').textContent=(names[s.service]||s.service)+'；'+(s.link==='configured'?'数据线已连接':s.link==='suspended'?'连接已挂起':'等待电脑连接');}catch(e){document.getElementById('usb-state').textContent='暂时无法读取 USB 状态';}}update();setInterval(update,10000);})();</script>
'''
page=original.replace('<main>','<main>\n'+panel,1).replace('Osmo Action · 网络设置','Osmo Action · 相机管理')
if REV>=20:page=(WEB/'index-v20.html').read_text(encoding='utf-8').replace('固件 v20',f'固件 v{REV}').replace('Firmware v20',f'Firmware v{REV}')
(WORK/'index.html').write_text(page,encoding='utf-8',newline='\n')
files={
 '/etc/init.d/S48rtmp':(WEB/'S48rtmp',0o100755),
 '/etc/init.d/S50service_dji':(WORK/'S50service_dji',0o100755),
 '/etc/init.d/S55rootshell':(WORK/'S55rootshell',0o100755),
 '/usr/bin/oa_cmd_start':(WEB/'oa_cmd_start',0o100755),
 '/usr/lib/oa_rtmp_bridge.so':(WEB/'oa_rtmp_bridge.so',0o100644),
 '/usr/local/www/cgi-bin/rtmp':(WEB/'rtmp-firmware',0o100755),
 '/usr/local/www/index.html':(WORK/'index.html',0o100644),
}
commands=[]
if REV >= 19:
    commands.append('mkdir /usr/lib/oa-usb')
    files.update({
      '/etc/init.d/S98usbnet':(WEB/'S98usbnet',0o100755),
      '/usr/bin/oa_usbnet':(WEB/'oa_usbnet',0o100755),
      '/usr/local/www/cgi-bin/usb-status':(WEB/'usb-status',0o100755),
    })
    for name,expected in {
      'dwc3-ambarella-internal48.ko':'74d5d66882b23ab6ff8a8ced4a8bf1ec8d6452a3cb227d5e3fc3a6cd295161ab',
      'usb_f_rndis-win.ko':'64ae29f8947e3083f6cbbadf89da0b7f1aefc61841b9ad12ea10aae73315d968',
      'g_ether-rndis-win.ko':'87f5f9a7c233233b7701c37714d9af1531038b4f1e1f0303fd6e8d0e2ab8223a',
    }.items():
        source=ROOT/'analysis_bins'/name
        assert hashlib.sha256(source.read_bytes()).hexdigest()==expected,name
        files['/usr/lib/oa-usb/'+name]=(source,0o100644)
existing={'/etc/init.d/S50service_dji','/etc/init.d/S55rootshell','/usr/local/www/index.html'}
if REV>=20:
    commands.append('mkdir /usr/lib/oa-stock')
    for old,name in [('/usr/bin/hostapd.dji','hostapd'),('/usr/bin/dji_wms_ac101.real','dji_wms_ac101')]:
        stock=WORK/('stock_'+name);stock.write_bytes(extract(old))
        files['/usr/lib/oa-stock/'+name]=(stock,0o100755)
    files['/etc/init.d/S98usbnet']=(WEB/'S98usbnet-disabled',0o100755)
    files['/usr/bin/oa_network']=(WEB/'oa_network',0o100755)
    files['/usr/bin/oa_network_api']=(WEB/'network',0o100755)
    files['/usr/local/www/cgi-bin/network']=(WEB/'network',0o100755)
    for name in ['S49network','S57wifi']:
        files['/etc/init.d/'+name]=(WEB/name,0o100755)
    for name in ['wifi_start.sh','wifi_stop.sh','sta_start.sh','ap_start.sh','wifi_mdev_action.sh']:
        target='/usr/local/share/script/'+name
        files[target]=(WEB/name,0o100755);existing.add(target)
    for target,name in [('/usr/bin/hostapd','hostapd'),('/usr/bin/dji_wms_ac101','dji_wms_ac101'),('/etc/wifiwatch.sh','wifiwatch.sh'),('/usr/local/www/cgi-bin/apply','retired-cgi'),('/usr/local/www/cgi-bin/status','retired-cgi')]:
        files[target]=(WEB/name,0o100755);existing.add(target)
    existing.add('/etc/init.d/S57wifi')
    for target,name in [('/usr/bin/oa_snake','oa_snake'),('/usr/bin/oa_snake_boot','oa_snake_boot'),('/etc/init.d/S99snake','S99snake')]:
        files[target]=(ROOT/'tools/snake'/name,0o100755)
for target,(source,mode) in files.items():
    if target in existing:commands.append(f'rm {target}')
    commands += [f'write {linux(source)} {target}',f'set_inode_field {target} mode 0{mode:o}']
batch=WORK/'debugfs.commands';batch.write_text('\n'.join(commands)+'\n',encoding='ascii')
log=run('debugfs','-w','-f',linux(batch),linux(image));(WORK/'debugfs.log').write_text(log)
manifest={}
for target,(source,mode) in files.items():
    restored=WORK/('verify_'+Path(target).name)
    run('debugfs','-R',f'dump {target} {linux(restored)}',linux(image))
    assert restored.read_bytes()==source.read_bytes(),target
    stat=run('debugfs','-R',f'stat {target}',linux(image))
    assert f'Mode:  0{mode&0o777:03o}' in stat,(target,stat)
    manifest[target]={'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'bytes':source.stat().st_size,'mode':oct(mode&0o777)}
    if source.read_bytes().startswith(b'#!/bin/sh'):run('sh','-n',linux(source))
fscheck=run('e2fsck','-fn',linux(image));(WORK/'e2fsck.txt').write_text(fscheck)
assert 'clean' in fscheck or 'blocks' in fscheck
# Original service and boot notification remain byte-identical.
for target,expected in [('/usr/bin/cmd_rlv_transfer',ROOT/'analysis_bins/cmd_rlv_transfer'),('/usr/bin/usb_conn_svr',ROOT/'analysis_bins/usb_conn_svr')]:
    assert extract(target)==expected.read_bytes()
assert '/usr/bin/rpc_ctrl_cln 2' in extract('/etc/init.d/S99bootdone').decode()
rfs=image.read_bytes();assert len(rfs)==len(oldpart)-256
out=bytearray(raw);out[pos+256:pos+length]=rfs;struct.pack_into('<I',out,pos,zlib.crc32(rfs)&0xffffffff)
crc=0xffffffff
for i,(off,n,old) in enumerate(parts):
    if i<4:assert out[off:off+n]==old
    crc=A.amba_calculate_crc32h_part(out[off:off+n],crc);struct.pack_into('<II',out,44+8*i,n,crc)
struct.pack_into('<I',out,32,crc^0xffffffff)
comp=lz4.frame.compress(bytes(out),block_size=lz4.frame.BLOCKSIZE_MAX4MB,block_linked=False,content_checksum=False,block_checksum=False,store_size=True,compression_level=9)
header=bytearray(src[:h.hdrend_offs]);h.set_ver_latest((1<<24)|(10<<16)|PATCH);header[:base]=bytes(h)
e.version=(1<<24)|(81<<16)|MODULE;e.dt_offs=h.hdrend_offs;e.stored_len=e.decrypted_len=len(comp)
e.stored_md5=e.decrypted_md5=(c_ubyte*16).from_buffer_copy(hashlib.md5(comp).digest());header[base:base+sizeof(e)]=bytes(e)
struct.pack_into('<H',header,len(header)-2,X.dji_calculate_crc16_part(header[:-2],0x3692))
p=OUT/f'OSMO_ACTION_FW_V01.10.00.{PATCH}.bin';p.write_bytes(header+comp)
check=p.read_bytes();hh=X.FwPkgHeader.from_buffer_copy(check);ee=X.FwPkgEntry.from_buffer_copy(check[base:]);payload=check[ee.dt_offs:ee.dt_offs+ee.stored_len]
assert X.dji_calculate_crc16_part(check[:hh.hdrend_offs-2],0x3692)==int.from_bytes(check[hh.hdrend_offs-2:hh.hdrend_offs],'little')
assert hashlib.md5(payload).digest()==bytes(ee.stored_md5);assert lz4.frame.decompress(payload)==bytes(out)
assert header[base+sizeof(e):-2]==src[base+sizeof(e):hh.hdrend_offs-2]
report={'package':p.name,'sha256':hashlib.sha256(check).hexdigest(),'bytes':len(check),'package_version':f'01.10.00.{PATCH}','module_version':f'01.81.{MODULE}','autostart_stream':False,'native_boot_handshake':True,'saved_handshake_included':False,'non_rootfs_partitions_identical':True,'header_crc16_verified':True,'module_md5_verified':True,'lz4_roundtrip_verified':True,'ext2_readonly_check_passed':True,'device_flash_tested':False,'files':manifest}
if REV>=20:report.update({'custom_wifi_modes':['sta'],'wifi_toggle_apply':'full_power_cycle','usb_ethernet_default':False,'usb_ethernet_retained':True,'native_snake_included':True,'web_languages':['zh-CN','en']})
(OUT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
for name in ['AC101_FW_DEBUG','OACT_FW_DEBUG','OSMO_ACTION_FW_DEBUG','OSMO_FW_DEBUG']:(OUT/name).write_bytes(b'')
print(json.dumps({k:v for k,v in report.items() if k!='files'},indent=2))
