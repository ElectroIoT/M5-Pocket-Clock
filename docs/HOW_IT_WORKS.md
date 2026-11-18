# How it works

Everything is in [`src/main.cpp`](../src/main.cpp).

## Time

1. **Wi-Fi setup** - with no saved Wi-Fi the clock starts a setup page (WiFiManager) on the access point
   `ClockSetup`. The page also has two options: *UTC offset* and *auto timezone* (1 = on).
2. **Sync** - a background task joins your Wi-Fi, finds the timezone, asks NTP servers
   (`pool.ntp.org`, `time.google.com`, `time.cloudflare.com`) for the time, then switches Wi-Fi off.
3. **Keeping time** - the ESP32 keeps the system time between syncs (a few seconds of drift per day).
   It re-syncs every **6 hours**, or **10 minutes** after a failure.
4. **Power cut** - the AtomS3 has no battery, so after a power cut it shows "connecting..." for a few
   seconds until it syncs again.

## Timezone

With auto timezone on, the clock asks `ip-api.com/json` (HTTP, free) for your UTC offset **including daylight
saving**, with `worldtimeapi.org` as a fallback. The last good value is saved, so a failed lookup keeps the old one.
Turn auto off in the setup page and enter a fixed UTC offset (for example `5.5`) to avoid the lookup.

## Display

The face is drawn in an off-screen canvas and pushed once per frame, so nothing flickers. All text is
fitted to the screen width (text wider than the screen would wrap around on this display).
Brightness is 130 by day and 18 between 22:00 and 06:00; picking the device up (accelerometer jolt) lifts
it to full for 8 seconds. Change `NIGHT_FROM`, `NIGHT_TO`, `BRIGHT_DAY`, `BRIGHT_NIGHT` at the top of `main.cpp`.

## Button

`BtnA` is the screen. Click = next face, release after 0.6 s = next colour, release after 3 s = Wi-Fi setup.
While you hold, the screen shows what will happen on release.

## Serial console (115200)

| Command | Effect |
|---|---|
| `ver` | print the version |
| `face 0-2`, `theme 0-5` | change face / colour |
| `settime H M S` | set the clock to 2026-11-18 H:M:S (used for the README pictures) |
| `sync` | sync now |
| `portal` / `stop` | open / close the Wi-Fi setup page |
| `wifireset` | forget the saved Wi-Fi and open setup |
| `shot` | send the last frame as raw RGB565 (used by `tools/shot.py`) |
