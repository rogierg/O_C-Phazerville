# MPC Mini MIDI implementation notes

This note captures the current understanding of how to integrate an AKAI MPC Mini as a direct USB MIDI controller into the Phazerville firmware tree.

## Summary

- The firmware treats MIDI as a first-class app, not as a runtime-loaded module.
- The relevant app is `AppCaptainMIDI`, enabled by `ENABLE_APP_MIDI`.
- The direct USB path is the correct implementation strategy for an MPC Mini.
- A separate dedicated app is the recommended approach for the MPC Mini control surface.
- The current implementation direction is control-surface-only for Teensy 4.1.

## Evidence in the repo

### App registration

The app list is compiled in `software/src/apps/_config.h`.

Relevant pattern:

```c++
#ifdef ENABLE_APP_MIDI
  , AppCaptainMIDI
#endif
```

This confirms that MIDI is an app in the main firmware app container.

### Generic MIDI app

The generic MIDI app is implemented in `software/src/apps/CaptainMIDI.h`.

It is declared as an app with the standard app macro:

```c++
OC_APP_CLASS(AppCaptainMIDI, TWOCCS("MI"), "Captain MIDI", "MIDI I/O")
```

The file also handles incoming USB MIDI traffic on the direct path.

### USB MIDI build flag

In `software/platformio.ini`, the general firmware environment enables USB MIDI:

```ini
build_flags =
  -DTEENSY_OPT_SMALLEST_CODE
  -DUSB_MIDI
```

For Teensy 4.1 board support, the repo also includes USB host MIDI support in the hardware environment.

## Direct MIDI architecture

The repo supports the following direct MIDI inputs in the MIDI app path:

- `usbMIDI`
- `usbHostMIDI[0]`
- `usbHostMIDI[1]`
- `MIDI1`

This makes the firmware capable of handling a direct USB MIDI controller such as the AKAI MPC Mini without using a browser or Web MIDI bridge.

## MPC Mini mapping direction

The current intended approach is:

- create a dedicated app for the MPC Mini pad matrix
- map the 64 pad grid to an 8x8 control surface model
- treat the app as a control surface only for now
- do not force this into the generic MIDI app

The app should listen for pad note on/off events and keep pad state in memory.

## Why a separate app is better

The built-in MIDI app is generic MIDI routing and transformation logic. It is not the right place to implement a custom pad-grid sequencer control surface.

A dedicated app keeps the responsibilities clean:

- `CaptainMIDI` = generic MIDI I/O and mapping
- `MPCMiniGrid` = specific pad matrix controller behavior

## Hardware target

The most relevant target for this feature is:

- Teensy 4.1
- direct USB MIDI input
- control surface app only
- no CV or gate generation in the first iteration

## Important limitation

This branch is not runtime-loadable from SD.

The firmware compiles apps into the binary at build time; there is no generic app loader from SD in this repo. That means the custom MPC Mini app must be included in the firmware build and flashed as part of a custom firmware image.

## Recommended next implementation step

Build a dedicated `AppMPCMiniGrid` app that:

- parses direct USB MIDI note messages
- maps notes to an 8x8 pad matrix
- keeps pressed/released state per pad
- exposes selected row/column state in the menu UI
- does not generate CV outputs yet

This keeps the project aligned with the repo architecture and the direct USB MIDI path.
