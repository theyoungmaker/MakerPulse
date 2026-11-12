# Changelog

## v1.2.0

- Added a "Screen orientation" setting (USB on top / USB on bottom) to the
  settings page, for the hardware variant where the USB cable enters from
  the bottom edge instead of the top. Hot-applies immediately, no reboot.
  One firmware image serves both orientations — the flashing site has no
  separate picker; see `docs/adr/0001-screen-orientation-runtime-setting.md`.

## v1.1.0

- Added a custom color-wheel/RGB picker for the Accent and Text colors on
  the settings page, alongside the existing named presets.
- Renamed the "Buddy nickname" field to "MakerPulse nickname".
- Firmware version now shown on the settings page footer and on the
  flashing site.
- Split the firmware source from a single file into separate modules
  (config, state, network, display, settings server).

## v1.0.0

- Initial release.
