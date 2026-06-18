# Current Hardware Setup

This file documents the currently built hardware state that matches `project/cad/Gunnis_mod.pdf`.

## Installed hardware

- Controller: Heltec WiFi LoRa 32 V2
- GPS: installed
- BME280: not installed
- VE.Direct: optional, not used together with BME280
- DS18B20: supported on 1-Wire
- Standby input: same signal as alarm input

## Pin mapping

- Battery voltage input: `GPIO36` (`AN0`)
- Tank 1 input: `GPIO37` (`AN1`)
- Tank 2 input: `GPIO38` (`AN2`)
- Alarm / standby input: `GPIO39` (`IN1`), active low
- Relay output: `GPIO25` (`OUT1`)
- Heltec board LED: `GPIO25`
- DS18B20 1-Wire: `GPIO23`
- GPS serial: `RXD2=GPIO12`, `TXD2=GPIO13`
- BME280 / VE.Direct shared bus pins: `GPIO17` and `GPIO22`

## Important hardware notes

- `GPIO25` is intentionally shared between the on-board LED and the relay control signal.
- BME280 and VE.Direct share pins and are alternative hardware options. They must not be active at the same time.
- The standby input is the same physical signal that is reported as alarm input in the firmware.
