# v21 · Osmo Action 1 RTMP Firmware

固件版本 **01.10.00.83**，模块版本 **01.81.7311**。仅适用于初代 DJI Osmo Action / AC101。

## 本版内容

- Wi-Fi STA 从内存卡 `wifi.conf` 读取配置；取消自定义 AP。
- 网页管理 RTMP 地址、码率以及手动启动/停止；重启不会自动推流。
- 网页支持简体中文和 English。
- STA 开关完整关机再开机生效；关闭后交回原厂 Wi-Fi，可使用 DJI Mimo。
- 原生触屏贪吃蛇固化，支持上下左右滑动；修复空提示框容器挡住游戏入口的问题。
- USB 网卡默认停用、网页隐藏，固件保留相关模块。

## 下载与安装

下载 `Action1_v21_STA_RTMP_Snake_i18n.zip` 并解压。将 BIN 与包内调试标记放在内存卡根目录，按 README 配置 `wifi.conf`。保持供电稳定，完整关机后开机，等待升级结束。不要混放多个升级 BIN。

首次使用请将网页默认推流目标修改为自己的 RTMP 服务器地址。电脑端的接收服务和低延迟修正不包含在固件中。

## 验证范围

当前 AC101 实机已完成冷启动 STA、网页与游戏自动加载、游戏滑动和退出、RTMP 手动启停与接收端解码。STA 关闭后的 Mimo 实时预览，以及删除关闭标记后的 STA 恢复，均已验证。

长时间录制及原厂 USB 文件传输的完整回归尚未覆盖。本固件保留无密码 root Telnet，仅用于可信局域网。

## 校验

固件 BIN SHA256：

```text
5b244f23bd099e0a6b94c749e93dbdcab51ca0723e16dca6f23d10f093e7bda6
```

ZIP SHA256：

```text
279c41372e3fb9f22a3b24aaf0d62fcb18095e462b8d896dd12c1c5e140c2adf
```

详见附件 `SHA256SUMS.txt`、包内使用说明及 `validation.json`。
