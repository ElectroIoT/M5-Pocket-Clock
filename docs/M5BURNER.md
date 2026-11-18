# Publishing to M5Burner

How to share this firmware in [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/publish)
(*USER CUSTOM > Publish*, logged in with an M5Stack community account).

> **Do not use M5Burner's "Export" on your own AtomS3.** Export reads the whole flash from the device,
> including your saved Wi-Fi name and password. Upload the clean `-full.bin` from the Releases page
> instead: it is built from source and its settings area (NVS) is blank.

## Fields (AtomS3)

| Field | Value |
|---|---|
| Name | `Pocket Clock` |
| Version | `v1.0.0` |
| Device Type | `ATOMS3` (AtomS3 with display; not AtomS3 Lite) |
| Github | `https://github.com/ElectroIoT/M5-Pocket-Clock` |
| FirmWare | `M5-Pocket-Clock-atoms3-v1.0.0-full.bin` (single image, flashed at `0x0`) |
| Cover | [`docs/images/m5burner_cover.png`](images/m5burner_cover.png) (800 x 600) |

## Firmware description (markdown, paste as is)

```markdown
# Pocket Clock

A **12-hour digital clock** for your **M5Stack AtomS3**, with AM/PM and the date.

## Features
- 3 watch faces (click the screen): **Digital**, **Ring** (seconds ring) and **Night** (dim, time only)
- 6 colours (hold the screen)
- **Sets itself over Wi-Fi once**, then re-syncs every 6 hours (Wi-Fi is off in between)
- **Finds your timezone automatically** (including daylight saving), or set a fixed UTC offset
- Dims at night (10 PM to 6 AM), full brightness when you pick it up
- Remembers face, colour, timezone and Wi-Fi

## First setup
1. Power it on: the screen says **WI-FI SETUP**
2. On your phone join the Wi-Fi **ClockSetup** (the setup page opens by itself, or go to 192.168.4.1)
3. Pick your home Wi-Fi, enter the password, press **Save**

Hold the screen 3 seconds to run setup again. For the **AtomS3 with display** (not AtomS3 Lite).
The AtomS3 has no battery, so after a power cut it re-syncs within seconds.

Source and guide: https://github.com/ElectroIoT/M5-Pocket-Clock
```

## Version description (markdown, paste as is)

```markdown
## v1.0.0 - first release

- 12-hour digital clock with AM/PM, seconds and date
- 3 watch faces (Digital, Ring, Night) and 6 colours
- One-time Wi-Fi setup from your phone (ClockSetup), automatic NTP sync every 6 hours
- Automatic timezone with daylight saving, or a fixed UTC offset
- Auto-dim at night, wakes to full brightness when picked up

Source and guide: https://github.com/ElectroIoT/M5-Pocket-Clock
```

## Making the image (maintainers)

```bash
pio run -e atoms3
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 merge_bin \
  -o release/M5-Pocket-Clock-atoms3-vX.Y.Z-full.bin --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 .pio/build/atoms3/bootloader.bin \
  0x8000 .pio/build/atoms3/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/atoms3/firmware.bin
```

Before uploading, check the image holds no personal data: `grep -c -a "<your Wi-Fi name>" release/*.bin`
must print `0` (Wi-Fi credentials live only in the device's NVS, never in the image), and the settings
area at `0x9000-0xDFFF` must be all `0xFF`.
