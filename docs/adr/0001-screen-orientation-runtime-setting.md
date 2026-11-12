# 0001. Screen orientation is a runtime setting, not a flashing-time choice

## Status

Accepted

## Context

A hardware variant of MakerPulse exists where the USB cable enters from the
bottom edge of the board instead of the top. The screen (and its touch
layer) need a different rotation on that variant, which previously lived as
a fixed compile-time constant (`ROT` in `src/config.h`), set the same way
for every board regardless of which edge its cable actually comes out of.

Three designs were considered for letting a user choose orientation:

1. **Two firmware binaries.** Build and release two PlatformIO environments
   per version — one per orientation — each with its own manifest and
   `docs/firmware/` folder. The flashing website picks which to install.
   Simple, but doubles the release process (`README.md`'s release
   checklist) forever, on every future version.

2. **Single firmware, custom serial handshake.** Keep one firmware image;
   have the flashing website send a one-time "set initial orientation"
   command over the same serial connection already used for Improv Wi-Fi
   provisioning, right after flashing completes. Investigation ruled this
   out: the vendored `ImprovWiFiLibrary`'s `handleSerial()` consumes the
   serial stream one byte at a time and silently discards anything that
   doesn't match its own `"IMPROV"` preamble — there's no passthrough for a
   custom protocol riding the same wire. Making this work would mean
   patching the vendored library to add a new command type *and* replacing
   `<esp-web-install-button>` with hand-rolled Web Serial flashing JS (no
   evidence the button exposes a hook for a page to inject extra commands
   into the wizard it owns internally). Separately, `tft.setRotation()`
   already runs in `setup()` before `loop()` ever calls `handleSerial()`
   for the first time, so even a successful handshake couldn't affect the
   very first screen draw on a fresh device — only a subsequent boot.

3. **Single firmware, runtime Preferences setting, no flashing-site
   picker.** Orientation becomes a setting read at boot and changeable any
   time afterward via the device's own browser-based Settings page — the
   same pattern already used for Accent/Text/Theme/timezone. The flashing
   website doesn't pick anything; it just gets a short instructional note
   telling bottom-cable users to flip the setting once, after Wi-Fi setup.

## Decision

Go with (3). Orientation is purely a Preferences-backed runtime setting
(`applyOrientationByKey()` in `src/state.cpp`), with a new "Screen
orientation" control on the Settings page. The flashing website's only
change is a copy addition — no picker, no second firmware build, no new
serial protocol.

## Consequences

- One firmware image, one manifest, one `docs/firmware/` folder per
  version, forever — no ongoing release-process cost.
- The cost is concentrated entirely in one place, once: a fresh "USB on
  bottom" device's on-device screen shows upside-down during the one-time
  initial setup screen (before Wi-Fi is configured, since there's no UI to
  fix it before that point). This is cosmetic and self-resolving — the
  settings page is a browser-rendered page, readable regardless of the
  physical screen's orientation, so the user corrects it there once and it
  hot-applies immediately (no reboot).
- `ROT`, `TOUCH_FLIP_X`, and `TOUCH_FLIP_Y` moved out of `src/config.h`
  (compile-time constants) into `src/state.h`/`state.cpp` as
  `rotationValue`/`touchFlipX`/`touchFlipY` (runtime, derived from
  `orientationKey`). `TOUCH_SWAP_XY` stayed a compile-time constant in
  `config.h` — it doesn't vary between the two orientations supported here
  (both are 180° apart on the same axis pair), only `TOUCH_FLIP_X/Y` do.
- The exact `rotationValue`/touch-flip values for "USB on bottom" are a
  best-effort guess (180° opposite of "USB on top" on this ST7789 panel),
  not yet verified against real bottom-cable hardware. See the comment
  above `applyOrientationByKey()` in `src/state.cpp`.
