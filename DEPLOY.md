# MDS OTA Deployment

Diese Datei beschreibt den aktuellen Deploy-Ablauf fuer Firmware und Webdateien.

## Zielbild

Der ESP nutzt nur noch den konfigurierten MDS-OTA-Endpunkt:

- Firmware OTA:
  - `https://mds-git.derguntmar.de/ota/getupdate.php`
- daraus automatisch abgeleitete Web-Dateien:
  - `https://mds-git.derguntmar.de/ota/bin/web/`

Es gibt keine separate Web-Update-Host-Konfiguration mehr auf dem ESP.

## Erwartete Serverstruktur

Private OTA-Dateien fuer `getupdate.php`:

```text
httpdocs/mds-git.derguntmar.de/var/ota/bin/
  firmware.bin
  V1.14l.bin
  firmware.version
  firmware.sha256
```

Oeffentliche OTA-Metadaten und Web-Dateien:

```text
httpdocs/mds-git.derguntmar.de/public/ota/bin/
  firmware.bin
  V1.14l.bin
  firmware.version
  firmware.sha256
  web/
    firmware-manifest.json
    webui-package.tar
    V1.14l/
      index.html
      settings.html
      firmware.html
      ...
      webui-package.tar
```

## Build

```bash
pio run
python3 scripts/build_web_bundle.py
```

Artefakte:

- [firmware.bin](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/firmware.bin)
- [webui-package.tar](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/webui-package.tar)

## Deploy

Standard:

```bash
./scripts/deploy_stable.sh
```

Das Script:

- liest `fversion` aus [src/Configuration.h](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/src/Configuration.h)
- laedt die Firmware nach `var/ota/bin/`
- laedt `firmware.version` und `firmware.sha256` nach:
  - `var/ota/bin/`
  - `public/ota/bin/`
- laedt das Web-Manifest nach:
  - `public/ota/bin/web/firmware-manifest.json`
- laedt alle Webdateien nach:
  - `public/ota/bin/web/<version>/`
- laedt das Web-Paket nach:
  - `public/ota/bin/web/webui-package.tar`
  - `public/ota/bin/web/<version>/webui-package.tar`

Dry run:

```bash
UPDATE_SERVER_FTP_HOST='' \
UPDATE_SERVER_FTP_USER='' \
UPDATE_SERVER_FTP_PASSWORD='' \
UPDATE_SERVER_FTP_DIR='' \
python3 scripts/deploy_update_server.py --dry-run
```

## Hinweise

- Der alte FTP-Webserver ist optional und kann komplett leer bleiben.
- Wenn der ESP bei MDS OTA trotzdem `304 Not Modified` bekommt, liegt das nicht mehr an den Dateien, sondern an der MDS-Serverlogik oder am Board-Flag `performUpdate`.
- Fuer die Anzeige der Server-Version im ESP muessen `public/ota/bin/firmware.version` und optional Header aus `getupdate.php` verfuegbar sein.
