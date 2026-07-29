# LoRaWAN payload contract

The firmware uses two application ports so recurring measurements stay separate
from rarely changing device metadata. The device MAC address is sent on FPort 2.
Neither packet contains Wi-Fi passwords, LoRaWAN session keys, MDS API keys, OTA
secrets or URLs.

## FPort 1: measurements

FPort 1 is sent at the configured LoRa interval. Schema 3 is exactly 51 bytes,
which is the EU868 DR2/SF10 application payload limit. The TTN decoder remains
backward compatible with the previous 33-byte and 50-byte payloads.

| Bytes | Value | Encoding |
| --- | --- | --- |
| 0 | schema version (`3`) | unsigned |
| 1-2 | uplink counter | unsigned, little endian |
| 3-4 | air temperature | Celsius x 10, signed |
| 5-6 | air pressure | mbar x 10 |
| 7 | humidity | percent |
| 8-9 | dew point | Celsius x 10, signed |
| 10-11 | battery voltage | volts x 1000 |
| 12-13 | DS18B20 temperature | Celsius x 10, signed |
| 14-17 | longitude | degrees x 1,000,000, signed |
| 18-21 | latitude | degrees x 1,000,000, signed |
| 22-23 | tank 1 and tank 2 level | percent |
| 24 | status flags | main-power-on bit 0 (legacy name `alarm1`), BME bit 2, VE.Direct bit 3, relay bits 4-5, GPS fix bit 6, WakeupLog bit 7 |
| 25 | battery capacity | percent |
| 26-29 | tank 1 and tank 2 raw ADC | unsigned, little endian |
| 30-33 | GPS speed and course | knots/degrees x 100 |
| 34-35 | GPS altitude | metres x 10, signed |
| 36-41 | VE.Direct voltage, current, temperature | x 100; current and temperature signed |
| 42-45 | standby timestamp | Unix epoch seconds, unsigned |
| 46-49 | wakeup timestamp | Unix epoch seconds, unsigned |
| 50 | standby/wakeup causes | standby in low nibble, wakeup in high nibble |

Standby cause `1` means `Sleep standby`. Wakeup causes are `1=EXT0`, `2=EXT1`,
`3=Timer`, `4=Touch`, `5=ULP`, `6=Other`; value `15` represents an unknown or
unmapped cause. Bytes 42-50 are valid only when status bit 7 is set.

The timestamps follow exactly the same firmware rules as the Wi-Fi
`WakeupStan/WakeupLog` record: standby is captured immediately before deep sleep,
and wakeup is captured at boot or reconstructed back to the boot moment after time
synchronization. TTN `received_at` is transport metadata only and must not replace
either device timestamp.

## FPort 2: device configuration

FPort 2 is sent after the first measurement following a cold boot and again only
when its configuration hash changes. It is 34 bytes.

| Bytes | Value |
| --- | --- |
| 0 | schema version (`1`) |
| 1 | standby, standby Wi-Fi, Wi-Fi upload, dynamic SF, mDNS, web auth and release-channel flags |
| 2-6 | config version, transmit interval, sleep duration, update interval |
| 7-13 | transmit priority, LoRa mode/SF/channel, server mode and sensor types |
| 14-21 | firmware version, ASCII and zero padded |
| 22-23 | device ID and relay mode |
| 24-29 | MAC address |
| 30-33 | FNV-1a hash of bytes 0-29 |

## Deliberate differences from Wi-Fi

The recurring sensor values and the paired standby/wakeup event sent to MDS over
Wi-Fi have a LoRa equivalent. HTTP-only operational information is not duplicated,
including OTA progress/errors and exact web-server state. A pending WakeupLog is
included in the next regular FPort 1 packet instead of creating a separate uplink.

## Transmission strategy

With `WifiFirst`, each due transmission is attempted through Wi-Fi/MDS first. A
complete successful MDS response replaces the corresponding LoRa uplink. If Wi-Fi
association, DNS, TLS, the MDS request or a required WakeupLog upload fails, the
firmware sends FPort 1 as fallback. WakeupLog packets use confirmed LoRaWAN
uplinks and remain queued until the network acknowledges them. A WakeupLog
acknowledged through one transport is removed from the other transport queue to
avoid duplicate event rows.

With `LoRaFirst`, FPort 1 is sent as a confirmed LoRaWAN uplink. A network ACK
marks the cycle as delivered and suppresses the Wi-Fi upload. If queuing fails,
the radio cycle times out or no ACK arrives, Wi-Fi/MDS is used as fallback. This
keeps the two priority modes symmetric and avoids duplicate measurement rows.

## TTN and MDS integration

- TTN must use `src/Payload_Formats_TTN_V3.js` as the application uplink formatter.
- Existing device registrations, ABP keys and webhook URLs do not change.
- The TTN device MAC settings and the firmware must use the same RX1 delay. This
  firmware uses one second (`Rx1 delay = 1`, `Desired Rx1 delay = 1`). After
  changing these values for an ABP device, reset its MAC state in TTN so the
  active session uses the new value; session keys and the device address remain
  unchanged.
- MDS accepts `payloadType=measurements` and `payloadSchema=3` for FPort 1 and
  maps a valid event to `WakeupStan/WakeupLog` with transmission path `2`.
- Status bit 0 is exposed as `mainPowerOn`: `1` means the battery main switch
  supplies 12 V and the device stays always on; `0` means sleep/wakeup mode is
  allowed. The decoder also emits `alarm1` as a temporary compatibility alias.
- MDS accepts `payloadType=deviceConfig` for FPort 2 and updates board metadata
  without creating zero-valued sensor rows.
- Schema 2 (50 bytes) and legacy 33-byte FPort 1 packets remain decodable.
