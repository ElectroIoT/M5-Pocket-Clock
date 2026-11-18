# Troubleshooting

| Problem | What to do |
|---|---|
| Screen says "WI-FI SETUP" | Join the Wi-Fi **ClockSetup** on your phone and pick your home Wi-Fi. |
| Setup page does not open | Open `http://192.168.4.1` in the phone browser while joined to ClockSetup. |
| Shows "no Wi-Fi" | The router is off or out of range; it retries every 10 minutes. Hold the screen 3 s to enter a new Wi-Fi. |
| Time is wrong by whole hours | Timezone lookup failed. Run setup again and set *auto timezone* to 0 and enter your UTC offset (India: 5.5). |
| Time is right but the sync dot is amber / red | The last sync is older than 12 hours / never happened. Check the Wi-Fi. |
| Shows "connecting..." after a power cut | Normal: the AtomS3 has no battery and re-syncs within seconds. |
| Screen too dim at night | Change `BRIGHT_NIGHT` or the night hours at the top of `src/main.cpp`. |
| No serial port appears | Use a USB-C **data** cable. Hold the screen button while plugging in USB (download mode), release after 2 s, retry. |
| Upload says *Wrong boot mode detected* | Put the device in download mode (hold the button, replug) and flash with `--before no_reset`. |
