# MakerPulse

An ESP32 "Cheap Yellow Display" (ESP32-2432S028) desk gadget — weather, bus
arrivals, focus timer, and status, with a touchscreen UI.

## Setup (end users)

No Arduino IDE needed. Visit the MakerPulse setup site in Chrome or Edge on
desktop, plug the device in over USB, and click **Connect and Flash
MakerPulse**. A WiFi setup dialog appears automatically right after flashing.
Revisit the same page any time to reconnect the device to a different network.

## Building from source

This is a [PlatformIO](https://platformio.org/) project.

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
1. `FIRMWARE_VERSION` in `src/main.cpp`
2. `"version"` in `docs/manifest.json` (and its `parts[].path` entries)
3. The `docs/firmware/vX.Y.Z/` directory name

Also update `SETUP_SITE_URL` in `src/main.cpp` once GitHub Pages is live, so
the on-device first-boot screen points at the real URL.

### Hosting

`docs/` is the GitHub Pages source directory (repo Settings → Pages → Deploy
from branch → `main` / `/docs`). Pure static files, no build step.

## Notes

- Renaming the Preferences namespace from `deskbuddy` to `makerpulse` means
  any device previously flashed with older firmware will have its stored
  settings reset once reflashed (Preferences namespaces are isolated
  storage; nothing migrates automatically).
- Bus arrivals use a shared fallback LTA DataMall AccountKey out of the box.
  For reliable results, request your own free key at
  https://datamall.lta.gov.sg/content/datamall/en/request-for-api.html and
  enter it on the device's settings page.
- WiFi provisioning uses the [Improv Wi-Fi](https://www.improv-wifi.com/)
  serial protocol and listens permanently — you can always reconnect the
  device to a new network from the setup website without re-flashing.
