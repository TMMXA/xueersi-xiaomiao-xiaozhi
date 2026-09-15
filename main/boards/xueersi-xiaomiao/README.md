# Xueersi Xiaomiao

XiaoZhi port for the ESP32-WROVER/B version of the Xueersi Xiaomiao programming
console with its original ST7735 display and six keys, plus an added ICS43434
microphone and MAX98357A speaker amplifier.

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

## Display

The original 128x160 ST7735 panel runs as a 160x128 landscape display on SPI2:

| Signal | GPIO |
| --- | ---: |
| SCLK | 18 |
| MOSI | 23 |
| MISO | 19 |
| CS | 5 |
| DC | 4 |

The panel has no dedicated reset or controllable backlight. MISO remains part
of the bus definition because the original microSD slot shares SPI2 and uses
GPIO22 for its own CS.

## Controls

| Key | GPIO | XiaoZhi action |
| --- | ---: | --- |
| A | 34 | Hold to talk; release to stop listening |
| B | 12 | Toggle chat; long press or press while starting to enter Wi-Fi setup |
| Up | 2 | Volume +10; long press for maximum volume |
| Down | 13 | Volume -10; long press to mute |
| Left | 27 | Select the light theme |
| Right | 35 | Select the dark theme |

GPIO34 and GPIO35 have no internal pull resistors, so their button instances
explicitly rely on the idle-high bias already present on the Xiaomiao PCB.

Wi-Fi provisioning starts automatically when no saved SSID is available, or
after a connection attempt times out.

## Scope and validation

The board implementation includes the XiaoZhi Wi-Fi/protocol stack, 24 kHz
full-duplex audio, PTT, ST7735 LVGL UI, status/chat/volume display, and all six
keys. The microSD slot is reserved but not mounted because XiaoZhi's core PTT
flow does not consume it. The abandoned MPU6050 repair is intentionally out of
scope.

The first-stage no-display image has booted on the device through the Wi-Fi
provisioning hotspot. The current display build compiles with ESP-IDF v6.1, but
still needs on-device validation of panel orientation/color, all keys, audio
capture/playback, server conversation, interruption, and reconnect behavior.
Local wake word and AEC remain disabled until that PTT baseline is verified.

## Build

Use ESP-IDF v6.1 and disable the wake word for the PTT build:

```sh
python scripts/build.py xueersi-xiaomiao --wake-word disabled
```

The resulting merged image is written to `build/merged-binary.bin`. Flash it at
offset `0x0`. Do not treat a successful build as hardware verification.
