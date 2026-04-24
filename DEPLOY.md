# Update Server Deployment

Diese Datei beschreibt den empfohlenen Ablauf, um Firmware und Webdateien auf den OTA-Update-Server zu veröffentlichen.

## Voraussetzungen

- FTP-Zugang zum Zielserver
- Zugriff auf das Zielverzeichnis des Webservers
- eine gebaute Firmware-Datei unter [firmware.bin](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/firmware.bin)

## Zielstruktur auf dem Server

Der ESP erwartet Inhalte unter:

```text
https://<firmwareUpdateUrl>/files_for_esp_webserver/
```

Beispiel:

```text
files_for_esp_webserver/
  latestVersion.txt
  latestStableVersion.txt
  ActualVersion.txt
  latestBetaVersion.txt
  V1.08o/
    firmware.bin
    index.html
    settings.html
    firmware_ota.html
    ...
```

## Stable deployen

```bash
./scripts/deploy_stable.sh \
  --host ftp.example.com \
  --user deploy-user \
  --password 'secret' \
  --remote-dir /var/www/html/files_for_esp_webserver
```

Das Script:

- liest die aktuelle `fversion` aus [src/Configuration.h](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/src/Configuration.h)
- kopiert `firmware.bin`
- kopiert alle Webdateien aus [data](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/data)
- aktualisiert:
  - `latestVersion.txt`
  - `latestStableVersion.txt`
  - `ActualVersion.txt`

## Beta deployen

```bash
./scripts/deploy_beta.sh \
  --host ftp.example.com \
  --user deploy-user \
  --password 'secret' \
  --remote-dir /var/www/html/files_for_esp_webserver
```

Das Script aktualisiert:

- `latestBetaVersion.txt`

## Komfort per Umgebungsvariablen

```bash
export UPDATE_SERVER_FTP_HOST='ftp.example.com'
export UPDATE_SERVER_FTP_USER='deploy-user'
export UPDATE_SERVER_FTP_PASSWORD='secret'
export UPDATE_SERVER_FTP_DIR='/var/www/html/files_for_esp_webserver'
export UPDATE_SERVER_FTP_PORT='21'
```

Dann reichen:

```bash
./scripts/deploy_stable.sh
./scripts/deploy_beta.sh
```

Du kannst dafuer auch die Vorlage [ .env.example ](/Users/guntmar/Documents/PlatformIO/Projects/LoRa-Boat-Monitor/.env.example) als Ausgangspunkt verwenden und die Werte in deine Shell-Umgebung uebernehmen.

Bequemes Laden:

```bash
cp .env.example .env
source ./scripts/source_env.sh
```

Optional mit anderem Dateinamen:

```bash
source ./scripts/source_env.sh ./deploy.env
```

## Automatisches Deploy nach Push

Wenn du nach jedem `git push` automatisch bauen und deployen willst:

```bash
./scripts/install_git_hooks.sh
```

Danach führt Git nach jedem Commit automatisch aus:

- `env PLATFORMIO_CORE_DIR=/tmp/pio-core pio run`
- `./scripts/deploy_stable.sh`

Ziel des Deployments:

```text
https://loraboatmonitorwebserverdata.derguntmar.de/files_for_esp_webserver
```

Voraussetzung:

- eine gültige `.env` im Projektverzeichnis

Wenn `.env` fehlt oder der Build fehlschlägt, wird der Push nicht rückgängig gemacht, aber das Auto-Deploy übersprungen bzw. mit Fehler beendet.

## Nützliche Optionen

Dry run:

```bash
./scripts/deploy_stable.sh --dry-run
```

Alternativer FTP-Port:

```bash
./scripts/deploy_stable.sh --port 2121
```

FTP über TLS:

```bash
./scripts/deploy_stable.sh --tls
```

## Verhalten auf dem ESP

- Die Firmware-Seite im Browser lädt keine Fremdserver-Dateien mehr direkt.
- Der Browser stößt nur einen lokalen Request an den ESP an.
- Der ESP löst die Stable- oder Beta-Version serverseitig auf und lädt die Firmware selbst per HTTPS.
- Stable akzeptiert aktuell diese Markerdateien:
  - `latestVersion.txt`
  - `latestStableVersion.txt`
  - `latestFirmwareVersion.txt`
  - `ActualVersion.txt`
- Der Inhalt darf entweder:
  - nur die Versionsnummer sein, z. B. `V1.08o`
  - oder direkt eine vollständige `https://.../firmware.bin`-URL

## Empfehlung

- Stable nur auf getestete Versionen zeigen lassen
- Beta separat über `latestBetaVersion.txt` steuern
- nach jedem Release zuerst `--dry-run` verwenden, wenn ein neues Serverziel eingerichtet wird
