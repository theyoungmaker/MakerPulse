# MakerPulse

MakerPulse is an ESP32 "Cheap Yellow Display" (ESP32-2432S028) desk
companion with a touchscreen UI: a clock, weather, air quality, bus
arrivals, and a focus timer, all customizable from a settings page the
device serves over WiFi.

Created by [The Young Maker](https://theyoungmaker.com) for its
[3D + micro-computing workshops](https://theyoungmaker.com/3dlab/).

## Data sources

- **Weather** (temperature, rain, wind, UV index, sunrise/sunset) comes
  from [Open-Meteo](https://open-meteo.com/), a free global forecast API.
- **PSI (haze index)** comes from Singapore's National Environment Agency
  (NEA), via the [data.gov.sg](https://data.gov.sg/) real-time API.
- **KP-index** (geomagnetic activity) comes from
  [NOAA's Space Weather Prediction Center](https://www.swpc.noaa.gov/).
- **Bus arrivals** come from [LTA DataMall](https://datamall.lta.gov.sg/).
  MakerPulse ships with a shared fallback AccountKey so this works out of
  the box, but shared keys can hit rate limits — request your own free key
  and enter it on the settings page for reliable results.

## Setup (end users)

No Arduino IDE needed. Visit the MakerPulse setup site in Chrome or Edge on
desktop, plug the device in over USB, and click **Connect and Flash
MakerPulse**. A WiFi setup dialog appears automatically right after flashing.
Revisit the same page any time to reconnect the device to a different network.

## Building from source

This is a [PlatformIO](https://platformio.org/) project. The firmware
source lives under `src/`, split by responsibility:

- `config.h` — pin assignments, display/touch calibration, layout
  constants, and shared value types.
- `state.h` / `state.cpp` — device settings, persisted preferences, and
  the small helpers that act on them.
- `network.h` / `network.cpp` — WiFi connection, Improv WiFi provisioning,
  and every outbound API fetch.
- `display.h` / `display.cpp` — everything drawn to the TFT and all touch
  handling.
- `settings_server.h` / `settings_server.cpp` — the device's own settings
  web page (served over WiFi).
- `main.cpp` — boot sequence and the main loop.

```bash
pio run -e makerpulse
```

### Producing a release build for the flasher website

```bash
pio run -e makerpulse
mkdir -p docs/firmware/vX.Y.Z
cp .pio/build/makerpulse/bootloader.bin docs/firmware/vX.Y.Z/
cp .pio/build/makerpulse/partitions.bin docs/firmware/vX.Y.Z/
cp .pio/build/makerpulse/firmware.bin   docs/firmware/vX.Y.Z/
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin docs/firmware/vX.Y.Z/
```

Bumping a release means updating all three of these together:
1. `FIRMWARE_VERSION` in `src/config.h`
2. `"version"` in `docs/manifest.json` (and its `parts[].path` entries)
3. The `docs/firmware/vX.Y.Z/` directory name

Add an entry to `CHANGELOG.md` for the new version. Also update
`SETUP_SITE_URL` in `src/config.h` if the GitHub Pages URL ever changes, so
the on-device first-boot screen keeps pointing at the right place.

### Hosting

`docs/` is the GitHub Pages source directory (repo Settings → Pages → Deploy
from branch → `main` / `/docs`). Pure static files, no build step. The
flashing site reads the current firmware version straight out of
`docs/manifest.json`, so it never drifts out of sync with what's flashable.

## Notes

- WiFi provisioning uses the [Improv Wi-Fi](https://www.improv-wifi.com/)
  serial protocol and listens permanently — you can always reconnect the
  device to a new network from the setup website without re-flashing.
- See `CONTEXT.md` for the settings-page/setup-site glossary and
  `CHANGELOG.md` for the release history.
