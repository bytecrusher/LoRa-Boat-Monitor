# Security Notes

## Implemented protections

- Configuration-changing requests require authentication and a CSRF token.
- Firmware and web packages require a non-default web password.
- MDS, OTA metadata, firmware, and web files are fetched only over HTTPS with certificate validation.
- Firmware and individual web files are verified with SHA-256 before installation.
- Web package extraction accepts only the known interface file names and uses rollback files.
- Passwords, Wi-Fi credentials, API keys, and OTA secrets are never inserted as plaintext into HTML templates or config backups.
- Browser responses include clickjacking, MIME-sniffing, referrer, and permission-policy headers.

## Deployment boundary

The ESP32 web interface uses HTTP Basic Authentication over plain HTTP. Basic Authentication protects access control, but it does not encrypt credentials or page contents in transit. Operate the interface only on the device access point, a trusted local network, or through an encrypted VPN/reverse proxy. Do not expose port 80 directly to the public internet.

The OTA flow validates TLS and SHA-256 integrity. It does not replace ESP32 Secure Boot or signed application images. Enabling Secure Boot and flash encryption is an irreversible production provisioning decision and must be performed with device-specific keys, backups, and a recovery procedure; it is intentionally not enabled automatically by this project.
