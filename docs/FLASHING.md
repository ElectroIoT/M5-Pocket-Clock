# Flashing & installing

Two devices are supported. Pick the right files:

| Device | Chip | Files (Releases page) |
|---|---|---|
| **M5Stack AtomS3** (with display; not AtomS3 Lite) | ESP32-S3 | `M5-Pocket-Clock-atoms3-v1.1.0-full.bin` / `-app.bin` |
| **M5StickC** (original; not Plus/Plus2) | ESP32 | `M5-Pocket-Clock-stickc-v1.1.0-full.bin` / `-app.bin` |

| File | Flash at | Use it for |
|---|---|---|
| `...-full.bin` | `0x0` | **first install** / M5Burner - bootloader + partitions + app in one file. Erases saved Wi-Fi. |
| `...-app.bin` | `0x10000` | **updates** - app only, **keeps** your Wi-Fi, face and colour |
| `SHA256SUMS.txt` | - | checksums |

## Option 1 - in the browser (easiest)

Open the **[web installer](https://electroiot.github.io/M5-Pocket-Clock/flasher/)** in Chrome or Edge on a
computer, connect the device, press **Install** under your device, pick the port. Uses
[ESP Web Tools](https://esphome.github.io/esp-web-tools/); nothing to install.

## Option 2 - M5Burner

Search "Pocket Clock" in M5Burner (device type *ATOMS3* or *STICKC*), download and burn.
(Maintainers: [M5BURNER.md](M5BURNER.md).)

## Option 3 - esptool (any OS)

```bash
pip install esptool
# AtomS3
esptool.py --chip esp32s3 --port COM9 --baud 460800 write_flash 0x0 M5-Pocket-Clock-atoms3-v1.1.0-full.bin
# M5StickC
esptool.py --chip esp32 --port COM18 --baud 115200 write_flash 0x0 M5-Pocket-Clock-stickc-v1.1.0-full.bin
```

Update and keep your Wi-Fi: use the `-app.bin` file at `0x10000` instead.

(`COM9` / `COM18` on Windows, `/dev/cu.usbmodem*` / `/dev/cu.usbserial-*` on macOS, `/dev/ttyACM0` / `/dev/ttyUSB0` on Linux.)

## Option 4 - PlatformIO (build from source)

```bash
git clone https://github.com/ElectroIoT/M5-Pocket-Clock
cd M5-Pocket-Clock
pio run -e atoms3 -t upload      # AtomS3
pio run -e m5stickc -t upload    # M5StickC
```

## After flashing

The screen shows **WI-FI SETUP** (AtomS3) or **SETUP** (StickC): join the Wi-Fi **ClockSetup** on your phone
and enter your home Wi-Fi.

## Download mode (AtomS3)

If the PC cannot see a port or the upload says *Wrong boot mode*: **hold the AtomS3 screen button**, plug in
USB, release after ~2 seconds (the screen stays blank), then flash again with `--before no_reset`.
Unplug and replug afterwards. The StickC normally flashes without any button.
