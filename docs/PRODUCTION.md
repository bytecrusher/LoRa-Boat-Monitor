# Production readiness

## Release flow

1. Develop and soak-test a beta build on hardware.
2. Run `python3 scripts/validate_project.py`, decoder tests and `pio run`.
3. Publish beta with the manual `Publish Firmware` GitHub workflow.
4. Verify OTA firmware, web package, Wi-Fi, TTN, MDS and standby cycling on a
   representative device.
5. Publish the same reviewed source as stable only after explicit approval.

The workflow uses pinned Python, PlatformIO and library versions. Publishing is
manual, channel-scoped and verifies the public OTA metadata, checksums and files
after upload. Configure the GitHub environments `beta` and `stable` with:

- `MDS_OTA_SSH_TARGET`
- `MDS_PUBLIC_OTA_SSH_TARGET`
- `DEPLOY_SSH_PRIVATE_KEY`
- `DEPLOY_SSH_KNOWN_HOSTS`

Protect the `stable` environment with a required reviewer.

## Recovery design

- Configuration is stored as checksum-protected CFG2 A/B records. The older
  legacy record remains a migration fallback.
- LittleFS config backup writes use temporary and previous files, so an interrupted
  backup does not silently replace the last valid copy.
- Firmware OTA records the previous app partition. A new image must remain healthy
  for 60 seconds or reach a controlled standby transition before it is confirmed.
- Three crash, watchdog or brownout resets during validation trigger the previous
  partition. Repeated crash resets also enable recovery mode with standby and
  automatic network work disabled so the local web interface remains reachable.
- Web package installation updates known web files transactionally and leaves the
  configuration backup untouched.

## Acceptance checklist

- Power-cycle during config save; one valid configuration must still load.
- Interrupt firmware and web-package updates at 10, 50 and 90 percent.
- Test valid, expired and wrong TLS chains plus checksum mismatches.
- Test all enabled Wi-Fi networks unavailable and recovery AP access.
- Test TTN unavailable, MDS unavailable and both unavailable.
- Run at least 100 standby/wakeup cycles and a 72-hour always-on soak.
- Verify frame counters never move backwards after resets or deep sleep.
- Verify FPort 1 and FPort 2 in TTN Live Data and MDS without duplicate rows.
- Check `/health` for reset reason, minimum heap, update state and boot validation.

## Security boundary

Use unique device passwords and rotate OTA/MDS secrets. The local UI uses HTTP
Basic Authentication without transport encryption and must stay on a trusted LAN,
the device AP or behind a VPN. Secure Boot and flash encryption are recommended
for manufactured devices but require device-specific key provisioning and a
tested recovery process; they are deliberately not enabled by a normal firmware
update because that provisioning step can be irreversible.

## Remaining production gates

Secure Boot/flash encryption, watchdog timeout selection and release promotion
must be validated on the final hardware revision. They should not be enabled
remotely without physical recovery access.

