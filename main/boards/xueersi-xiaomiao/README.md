# Xueersi Xiaomiao

First-stage XiaoZhi port for the ESP32-WROVER/B version of the Xueersi
Xiaomiao programming console with an added ICS43434 microphone and MAX98357A
speaker amplifier.

See [the project handoff](../../../docs/xueersi-xiaomiao-handoff.md) for the
verified hardware history and the scope of later integration stages.

## Audio wiring

| Signal | GPIO |
| --- | ---: |
| Shared I2S WS / LRCLK | 25 |
| Shared I2S BCLK / SCK | 26 |
| ICS43434 data to ESP32 | 32 |
| ESP32 data to MAX98357A | 33 |

Both directions use the same I2S controller and run at 24 kHz. Do not configure
different input and output sample rates while the clock pins are shared.

## Controls

The on-board A button on GPIO34 is push-to-talk:

- Press and hold: start listening.
- Release: stop listening.

Wi-Fi provisioning starts automatically when no saved SSID is available, or
after a connection attempt times out.

## Scope

This first-stage board intentionally uses `NoDisplay`. The original ST7735
display, the remaining five buttons, the microSD slot, and other peripherals
will be integrated only after Wi-Fi and bidirectional audio have been verified
on hardware.

## Build

Use ESP-IDF v6.1 and disable the wake word for the first-stage PTT build:

```sh
python scripts/build.py xueersi-xiaomiao --wake-word disabled
```

The resulting merged image is written to `build/merged-binary.bin`. Flashing
and on-device audio validation are still required before treating this port as
hardware-verified.
