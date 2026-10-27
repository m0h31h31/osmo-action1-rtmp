# Osmo Action 1 RTMP Firmware

[简体中文](README.md) | **English**

Custom firmware for the original **DJI Osmo Action (AC101)**, enabling RTMP streaming over Wi-Fi STA with browser-based stream management.

Features include bilingual web controls and a native touchscreen Snake game. This is a community modification project and is not affiliated with DJI.

**[Download firmware / Releases](https://github.com/m0h31h31/osmo-action1-rtmp/releases/latest)** · [Configuration example](examples/wifi.conf.example) · [Build instructions (Chinese)](docs/BUILD.md) · [Hardware validation record](docs/v21-validation.json)

## Features

- **RTMP streaming:** Save the destination URL and bitrate, manually start or stop streaming, and query camera status. Streaming never starts automatically at boot.
- **Wi-Fi STA:** Read credentials from `wifi.conf` on the SD card. The custom networking feature supports STA only; custom AP mode has been removed.
- **Stock Wi-Fi handoff:** Disable STA in the web interface, fully power off and power on, and let the stock system manage Wi-Fi for DJI Mimo.
- **Bilingual web interface:** Simplified Chinese and English, with the language choice remembered by the browser.
- **Native Snake game:** Launch from the camera's system settings, swipe in four directions to steer, and start, restart, or exit on the touchscreen.
- **USB components retained:** USB Ethernet modules and tools remain in the firmware, but do not start by default and have no web controls.

This project does not provide a UVC webcam mode for direct connection to OBS.

## Supported hardware and version

| Item | Current scope |
| --- | --- |
| Camera | Original DJI Osmo Action / AC101 |
| Custom firmware | v21 |
| Firmware version | `01.10.00.83` |
| Module version | `01.81.7311` |

Not intended for Action 2, Osmo Action 3, 4, 5, or other models. Validation was performed on the project's AC101 camera. The game loader checks signatures of the expected system image and cannot be directly ported to other firmware versions.

## Installation

Use a release package matching AC101 and back up important SD card files first.

1. Download and extract the firmware ZIP from Releases.
2. Copy the BIN and the included debug marker files to the SD card root.
3. Configure `wifi.conf` as described below.
4. With sufficient battery charge and a stable power supply, fully power off the camera and power it on again.
5. Wait for the upgrade to finish. Do not remove the battery or interrupt power during the upgrade.
6. Once the camera returns to shooting preview, check networking and the web interface.

The package's `validation.json` records checksums and validation scope. Do not place multiple upgrade BIN files for different versions in the SD card root at the same time.

## Wi-Fi configuration

Create a UTF-8 text file named `wifi.conf` in the SD card root:

```ini
WIFI_MODE=sta
ESSID="Your Wi-Fi name"
PASSWORD="Your Wi-Fi password"
```

Supports WPA/WPA2 Personal with an 8–63-byte passphrase or a 64-digit hexadecimal PSK. Use `PASSWORD=` for an open network. Both LF and CRLF line endings are accepted.

Values are read literally: commands are not executed and variables are not expanded. Do not append inline comments to values. Enterprise authentication, WEP, and WPA3-only networks are not currently supported.

After boot, find the camera in your router's client list and open `http://<camera-IP>/`. Each camera stores its own STA MAC address. A missing or invalid STA configuration does not create a custom access point as a fallback.

## STA enable/disable control

Changes saved in the web interface apply after a **full power-off and power-on**. Turning the screen off and waking it again does not apply the change.

With STA disabled, the camera no longer joins the configured router, so its previous web address becomes unavailable. The stock system manages Wi-Fi and DJI Mimo can be used. Only the custom AP feature has been removed; the access point required by stock connectivity is retained.

### Restore STA using the SD card

1. Power off the camera.
2. Delete `OA_STA_DISABLE` from the SD card root.
3. Delete the legacy `NO_WIFI` file too, if present.
4. Check that `wifi.conf` is valid, then power on.

When booting without an SD card, Wi-Fi is managed by the stock system.

## RTMP and OBS

1. Run an RTMP receiver, such as MediaMTX, on your computer or server.
2. Enter its URL in the camera's web interface, for example `rtmp://192.168.1.10:1935/live/osmo`.
3. Save the settings and click **Start stream**.
4. Add the corresponding receiving source in OBS.

The firmware's default destination comes from the development environment. Change it to your own server address before first use.

The camera reporting "Streaming" does not confirm that the server is receiving video; check the receiver as well. Low-latency timestamp correction runs on the computer and is **not included in the firmware**. The `osmo-lowdelay` URL shown on the page works only when the corresponding receiver and correction service are running. Actual latency depends on the network, receiver, and OBS buffering settings.

## Native Snake game

Open the camera's system settings and tap **贪吃蛇** (Snake). After starting, swipe on the board to steer; no direction buttons are needed. The game yields control when the camera is busy or a real stock dialog appears.

To disable the game, create an empty `OA_SNAKE_DISABLE` file in the SD card root and fully power off and power on. Delete the file and repeat the power cycle to re-enable it. The loader may retain a failure marker after detecting a problem for troubleshooting.

## Verified behavior

The following checks were completed on the current AC101 camera:

- STA reconnects automatically after upgrade and a full power cycle; the web interface and game load automatically.
- Game entry, swipe controls, and exit.
- Manual RTMP start/stop through the web interface, with actual video decoding at the receiver.
- DJI Mimo connects and displays live preview after STA is disabled.
- STA reconnects after deleting the disable marker and power-cycling.
- Chinese/English switching and language persistence; USB Ethernet does not load automatically.

Comprehensive regression testing of long-duration recording and stock USB file transfer has not been completed.

### v21 fix

Fixed a case where an empty stock dialog container left the Snake entry visible but prevented entering the game. Real-dialog and camera-busy protection remain enabled.

## Repository layout

```text
tools/rtmp_web/           Web UI, RTMP control, and STA management sources
tools/snake/              Native game, loader, and tests
build_rtmp_firmware.py    Firmware builder and integrity checks
examples/                Configuration examples without private data
docs/                    Build instructions and validation records
```

The repository does not include private network configurations, camera runtime logs, or complete base images. Building requires local inputs with matching checksums; cloning this repository alone is not enough to produce a complete firmware image. See the [build instructions (Chinese)](docs/BUILD.md). Firmware downloads are available in Releases.

## Scope and security

This modified firmware includes low-level system changes and carries no official compatibility or warranty assurance.

The firmware retains a **passwordless root Telnet debugging interface**. Use it only on a trusted local network, and do not forward camera management ports to the Internet. Remove Wi-Fi passwords, stream keys, and other private configuration before submitting an issue or sharing logs.

## Third-party components

Firmware packing tools retain their original authorship and GPL-3.0-or-later notices; see [THIRD_PARTY.md](THIRD_PARTY.md). Licenses for third-party components, vendor firmware, and newly added project code do not replace one another. No single license has yet been declared for all newly added project code.
