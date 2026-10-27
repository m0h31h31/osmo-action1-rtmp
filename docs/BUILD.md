# 构建与测试

当前打包脚本使用 Windows Python 调用名为 `Ubuntu-24.04` 的 WSL 发行版。它是已使用的开发流程，并非任意平台的一键构建器。

## 依赖

- Windows Python：`pip install -r requirements.txt`。
- WSL：Clang/LLD/LLVM 18、AArch64 Linux 交叉编译 sysroot（`/usr/aarch64-linux-gnu`）、`debugfs`、`e2fsck`。
- WSL Python：运行测试所需的依赖；网页状态测试还使用 Node.js。

## 本地输入

下列基础镜像/模块未提交到 Git；须自行准备项目对应版本，不能用其他固件随意替换：

| 路径 | 用途 |
| --- | --- |
| `analysis_bins/v15.bin` | 已有 v15 修改基础包，SHA256 `1d019cc373ed0ddc5f033b0a6f3c1c4cac81be5dc9091c15ca3ebcc7dd9deaab` |
| `analysis_bins/rootfs_v15.img` | 与该基础包一致的 rootfs |
| `analysis_bins/rtos_decompressed.bin` | SHA256 `662bc2f4f8700bb0edd82af26c93d8efd94a75045e632b12ce635920648bfa92` |
| `analysis_bins/cmd_rlv_transfer`、`usb_conn_svr` | 用于验证原生组件未改动 |
| `analysis_bins/dwc3-ambarella-internal48.ko` | 保留的 USB 控制器模块 |
| `analysis_bins/usb_f_rndis-win.ko`、`g_ether-rndis-win.ko` | 保留的 USB 网卡模块 |

模块的预期校验值直接写在 `build_rtmp_firmware.py` 中。该仓库没有从原厂包重建 v15 的完整历史流程，不应把当前流程理解为完全自包含的可复现构建。

## 编译组件

在 WSL 的仓库根目录运行：

```sh
sh tools/build_components.sh
```

游戏的 `loader_payload.h` 从固定 RTOS 输入生成，不作为源码提交。`labels.h` 已包含游戏所用文字位图，构建不需要安装字体。

然后在 Windows 的仓库根目录运行：

```powershell
python build_rtmp_firmware.py --revision 21
```

脚本只生成并校验升级包，不连接或刷写相机。输出位于 `firmware/flash_root_v21/`。

## 离线测试

在 WSL 的仓库根目录运行：

```sh
python3 tools/rtmp_web/test_network.py
python3 tools/rtmp_web/test_network_boot.py
node tools/rtmp_web/test_page.cjs
sh tools/snake/check.sh
# 下列测试需要先生成游戏 payload 和 loader_payload.h：
python3 tools/snake/test_adapter.py
python3 tools/snake/test_loader.py
```

测试使用隔离的临时文件或模拟器，不访问真实相机内存。镜像打包检查不能代替实机升级与功能验收。构建器新生成的 `validation.json` 默认将 `device_flash_tested` 标记为 false；仓库中的发布验证记录来自已完成的实机检查。
