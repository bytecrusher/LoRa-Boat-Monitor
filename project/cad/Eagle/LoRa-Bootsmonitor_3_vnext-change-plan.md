# LoRa-Bootsmonitor vNext Change Plan

This file is the implementation-oriented follow-up to:

- [../Gunnis_mod.pdf](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/project/cad/Gunnis_mod.pdf)
- [../../docs/hardware-current-setup.md](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/project/docs/hardware-current-setup.md)
- [../../docs/hardware-improvement-proposal.md](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/project/docs/hardware-improvement-proposal.md)

It is intended as the concrete checklist for the next schematic revision in Eagle.

## Scope

Keep the existing device architecture, but improve:

- relay / LED separation
- 12 V input robustness
- ADC stability
- documentation of optional hardware

## Main schematic changes

## 1. Move relay control away from GPIO25

### Current

- `GPIO25` from the Heltec module drives:
  - the Heltec on-board LED
  - the relay driver transistor input

### vNext

- keep `GPIO25` only as on-board LED signal
- move relay control net to `GPIO33`

### Schematic action

- change the relay driver input net from `OUT1` / `GPIO25` to `GPIO33`
- rename the relay control net to `RELAY_CTRL`
- remove any functional dependency on `GPIO25` for relay switching

### Firmware follow-up

- update `relayPin` from `25` to `33`
- keep LED handling on `25`

## 2. Improve 12 V input stage

### Current

- 12 V input
- diode-based protection
- LM2596 buck module
- TVS-like protection already indicated

### vNext

Recommended chain:

1. input connector
2. fuse
3. reverse polarity protection
4. TVS diode to GND
5. buck converter
6. local bulk capacitor on 5 V rail

### Schematic action

- add fuse `F1` at the 12 V input
- replace or complement series-diode reverse protection with a P-channel MOSFET stage if board space allows
- keep or upgrade transient clamp device near input connector
- place one electrolytic and one ceramic capacitor close to the LM2596 output

### Suggested parts

- `F1`: 1 A to 2 A fuse, depending on actual load
- reverse polarity: P-MOSFET ideal diode stage
- TVS: automotive / marine suitable 600 W class or similar, chosen for 12 V system
- output capacitors:
  - `100 uF` electrolytic
  - `100 nF` ceramic

## 3. Add RC filtering to analog inputs

### Affected nets

- `BAT_ADC`
- `TANK1_ADC`
- `TANK2_ADC`

### Schematic action

For each ADC input:

- add a series resistor between source network and MCU ADC pin
- add a capacitor from ADC pin to GND

### Suggested values

- battery ADC:
  - series resistor `1k`
  - capacitor `100 nF`
- tank ADCs:
  - series resistor `1k` to `4.7k`
  - capacitor `10 nF` to `100 nF`

### Net naming

- `BAT_ADC_RAW` -> divider output before series resistor
- `BAT_ADC` -> MCU-side filtered node
- same pattern for `TANK1_ADC` and `TANK2_ADC`

## 4. Keep standby input active-low and clearly labeled

### Current

- optocoupler input
- same signal used as alarm and standby control

### vNext

- keep this design
- document it more clearly in the schematic

### Schematic action

- rename MCU-side signal to `STANDBY_ALARM_N`
- add note: `active low, 0 V at input = active`
- add a test point on MCU side

## 5. Clarify BME280 and VE.Direct mutual exclusivity

### Current

- both options share `GPIO17` and `GPIO22`

### vNext

- keep shared pins if this is intentionally optional hardware
- show this clearly in the schematic

### Schematic action

- mark `BME280` as `optional / DNP`
- add note: `BME280 and VE.Direct share pins; populate only one functional option`
- if desired, add solder jumpers:
  - `SJ1` for SDA / RX path selection
  - `SJ2` for SCL / TX path selection

## 6. Improve relay driver annotation

### Current

- transistor driver with relay coil and diode

### vNext

- keep topology if switching current is modest
- make suppression and current path obvious in the schematic

### Schematic action

- label transistor stage as `RELAY DRIVER`
- ensure flyback diode is drawn directly across relay coil
- keep base resistor explicit
- optionally add test point on transistor collector or relay control net

## Proposed net naming cleanup

Replace generic names with clearer function names where possible.

| Current style | Proposed name |
|---|---|
| `OUT1` | `RELAY_CTRL` |
| `IN1` | `STANDBY_ALARM_N` |
| `AN0` | `BAT_ADC` |
| `AN1` | `TANK1_ADC` |
| `AN2` | `TANK2_ADC` |
| `1WIRE` | `TEMP_1WIRE` |
| `+5V` | `+5V_SYS` |
| `+3.3V` | `+3V3_MCU` |

## Suggested new test points

- `TP1`: `12V_IN`
- `TP2`: `+5V_SYS`
- `TP3`: `GND`
- `TP4`: `BAT_ADC`
- `TP5`: `TANK1_ADC`
- `TP6`: `TANK2_ADC`
- `TP7`: `STANDBY_ALARM_N`
- `TP8`: `RELAY_CTRL`
- `TP9`: `GPS_TX`
- `TP10`: `GPS_RX`

## Proposed vNext MCU signal table

| Function | Current | vNext |
|---|---:|---:|
| Board LED | GPIO25 | GPIO25 |
| Relay output | GPIO25 | GPIO33 |
| Battery ADC | GPIO36 | GPIO36 |
| Tank 1 ADC | GPIO37 | GPIO37 |
| Tank 2 ADC | GPIO38 | GPIO38 |
| Standby / alarm input | GPIO39 | GPIO39 |
| DS18B20 | GPIO23 | GPIO23 |
| GPS UART RX/TX | GPIO12 / GPIO13 | GPIO12 / GPIO13 |
| Optional BME280 / VE.Direct | GPIO17 / GPIO22 | GPIO17 / GPIO22 |

## Change checklist for Eagle

- update relay net from `GPIO25` to `GPIO33`
- rename generic nets
- add fuse symbol and device
- add reverse polarity protection stage
- verify TVS part selection
- add RC filters for all three ADC inputs
- add optional / DNP note for BME280
- add BME280 / VE.Direct mutual exclusion note
- add test points
- regenerate PDF and part list

## Deliverables for the next CAD pass

When this is implemented in Eagle, export:

- updated `.sch`
- updated `.brd`
- updated schematic PDF
- updated part list
- optional board render screenshot
