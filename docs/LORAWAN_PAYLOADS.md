# LoRaWAN payload contract

The firmware sends compact measurements on FPort 3 and metadata on FPort 2.
FPort 1 remains supported for older firmware. The MAC is included in every new
measurement packet, not only in the less reliable one-off metadata packet.
No packet contains Wi-Fi passwords, LoRaWAN session keys, MDS API keys, OTA
secrets or URLs.

## FPort 3: compact measurements (schema 4)

The header is 11 bytes. Byte 0 is `4`, byte 1 is the presence mask, bytes 2-7
are the MAC in display order, bytes 8-9 are the little-endian uplink counter,
and byte 10 contains main-power bit 0 and relay bits 4-5. Other status bits are
reserved. A MAC identifies a device; it is not an authentication credential.

Present blocks follow in this exact order. All integers are little endian;
scaling and signedness match schema 3 below. Absent blocks have no bytes and
must not produce zero readings or refresh the corresponding sensor timestamp.

| Mask bit | Block | Bytes | Schema-3 source offsets, in order |
| --- | --- | --- | --- |
| 6 | Battery voltage and capacity | 3 | 10-11, 25 |
| 0 | Valid DS18B20 temperature | 2 | 12-13 |
| 1 | Environment: temperature, pressure, humidity, dewpoint | 7 | 3-9 |
| 2 | GPS fix: longitude, latitude, speed, course, altitude | 14 | 14-21, 30-35 |
| 3 | VE.Direct voltage, current, temperature | 6 | 36-41 |
| 4 | Tank levels and raw ADC values | 6 | 22-23, 26-29 |
| 5 | Standby/wakeup timestamps and packed causes | 9 | 42-50 |

Bit 7 is reserved. Unknown versions, reserved mask bit 7, truncated blocks,
trailing bytes and packets over 51 bytes are rejected, including by MDS's raw
decoder. FPort 3 avoids ambiguous detection against old FPort-1 packets whose
first byte is an arbitrary counter. Decoder output uses `payloadSchema=4` and
explicit presence booleans; unavailable measurement fields are omitted.

Normal packets need at most 49 bytes. Battery + DS18B20 + tanks needs 22 bytes,
including the MAC, instead of the old fixed 51 bytes (31 with an event).
Available optional groups are also included when they fit. If a packet would
exceed 51 bytes, environment/GPS/VE.Direct/tanks are omitted in rotating order.
Battery, DS18B20 and events always have priority; events are never deferred just
to make space. Rotation survives deep sleep, so no optional group is starved by
repeated event packets. This does not change the sensor
sampling interval or guarantee a particular radio airtime.

DS18B20 errors are unavailable, never the previous value or a manufactured zero.
Zero Celsius is valid. All transmitted temperatures remain Celsius even when
the local display uses Fahrenheit. Wi-Fi uploads likewise omit invalid values
and GPS without a valid fix. Stable `bm<MAC><sensor-key>` addresses are sent
even before the first metadata-sync success; names remain a fallback.

## FPort 1: previous measurement format

Older firmware sends FPort 1 at the configured LoRa interval. Schema 3 is exactly 51 bytes,
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

FPort 2 is queued after an eligible measurement following a cold boot, when its
configuration hash changes, or after 24 hours since its last transmission when
a valid clock is available. It is 34 bytes and unconfirmed; transmission does
not prove receipt. Repetition permits recovery after a lost metadata packet.

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
included in a regular FPort 3 packet instead of creating a separate uplink.

## Transmission strategy

With `WifiFirst`, each due transmission is attempted through Wi-Fi/MDS first. A
complete successful MDS response replaces the corresponding LoRa uplink. If Wi-Fi
association, DNS, TLS, the MDS request or a required WakeupLog upload fails, the
firmware sends FPort 3 as fallback. WakeupLog packets request confirmation only
when the confirmation cooldown permits it and remain pending without an ACK. A WakeupLog
acknowledged through one transport is removed from the other transport queue to
avoid duplicate event rows.

With `LoRaFirst`, FPort 3 requests confirmation when the cooldown permits it. A network ACK
suppresses the Wi-Fi upload. If queuing fails,
the radio cycle times out or no ACK arrives, Wi-Fi/MDS is used as fallback. This
can produce duplicate arrivals, especially when confirmation is suppressed;
the server must tolerate both paths. A network ACK is not proof of MDS database
storage: true end-to-end delivery requires an application acknowledgement.

Confirmed requests (including manual tests) have a three-hour cooldown retained
across deep sleep. Without a valid clock, only one is requested until a valid
clock starts the cooldown. Cold power loss resets this in-memory policy. LMIC
may retransmit a confirmed frame; this cooldown is not a hard airtime/downlink
quota. Choose the uplink interval/data rate for the applicable network policy.

## TTN and MDS integration

- TTN must use `src/Payload_Formats_TTN_V3.js` as the application uplink formatter.
- Roll out **MDS first, TTN formatter second, firmware last**. Remove outdated
  device-level formatter overrides or replace them too. Allow FPort 3 in any
  custom integration filters. No changes to keys or re-registration are needed.
- Existing device registrations, ABP keys and webhook URLs do not change.
- The TTN device MAC settings and the firmware must use the same RX1 delay. This
  firmware uses one second (`Rx1 delay = 1`, `Desired Rx1 delay = 1`). After
  changing these values for an ABP device, reset its MAC state in TTN so the
  active session uses the new value; session keys and the device address remain
  unchanged.
- MDS accepts `payloadType=measurements` and `payloadSchema=4` for FPort 3, or `3` for FPort 1, and
  maps a valid event to `WakeupStan/WakeupLog` with transmission path `2`.
- Status bit 0 is exposed as `mainPowerOn`: `1` means the battery main switch
  supplies 12 V and the device stays always on; `0` means sleep/wakeup mode is
  allowed. The decoder also emits `alarm1` as a temporary compatibility alias.
- MDS accepts `payloadType=deviceConfig` for FPort 2 and updates board metadata
  without creating zero-valued sensor rows.
- Schema 2 (50 bytes) and legacy 33-byte FPort 1 packets remain decodable.
