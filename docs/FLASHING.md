# Flashing & installing

Target: **M5Stack AtomS3** (ESP32-S3, 0.85" 128x128 display). Not for AtomS3 Lite (no display).

| File (Releases page) | Flash at | Use it for |
|---|---|---|
| `M5-Pocket-Clock-atoms3-v1.0.0-full.bin` | `0x0` | **first install** / M5Burner - bootloader + partitions + app in one file. Erases saved Wi-Fi. |
| `M5-Pocket-Clock-atoms3-v1.0.0-app.bin` | `0x10000` | **updates** - app only, **keeps** your Wi-Fi, face and colour |
| `SHA256SUMS.txt` | - | checksums |

## Option 1 - in the browser (easiest)

Open the **[web installer](https://electroiot.github.io/M5-Pocket-Clock/flasher/)** in Chrome or Edge on a
computer, connect the AtomS3, press **Install**, pick the port. Uses
[ESP Web Tools](https://esphome.github.io/esp-web-tools/); nothing to install.

## Option 2 - M5Burner

Search "Pocket Clock" in M5Burner (device type *ATOMS3*), download and burn.
(Maintainers: [M5BURNER.md](M5BURNER.md).)

## Option 3 - esptool (any OS)

```bash
pip install esptool
esptool.py --chip esp32s3 --port COM9 --baud 460800 write_flash 0x0 M5-Pocket-Clock-atoms3-v1.0.0-full.bin
```

Update and keep your Wi-Fi:

```bash
esptool.py --chip esp32s3 --port COM9 --baud 460800 write_flash 0x10000 M5-Pocket-Clock-atoms3-v1.0.0-app.bin
```

(`COM9` on Windows, `/dev/cu.usbmodem*` on macOS, `/dev/ttyACM0` on Linux.)

## Option 4 - PlatformIO (build from source)

```bash
git clone https://github.com/ElectroIoT/M5-Pocket-Clock
cd M5-Pocket-Clock
pio run -e atoms3 -t upload
```

## After flashing

The screen shows **WI-FI SETUP**: join the Wi-Fi **ClockSetup** on your phone and enter your home Wi-Fi.

## Download mode

If the PC cannot see a port or the upload says *Wrong boot mode*: **hold the AtomS3 screen button**, plug in
USB, release after ~2 seconds (the screen stays blank), then flash again with `--before no_reset`.
Unplug and replug afterwards.
