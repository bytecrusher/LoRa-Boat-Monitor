#include "func_webclient.h"
#include <WiFi.h>       // WiFi lib with TCP server and client
#include <WiFiClient.h> // WiFi lib for clients
#include <WiFiClientSecure.h>

#include "FS.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <esp_sleep.h>
#include <freertos/semphr.h>
#include <time.h>
#include <Configuration.h>
#include "func_myFunctions.h"
#include "func_webServerHandler.h"
#include "updateRuntimeState.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");
extern time_t lastStandbyEventEpoch;
extern time_t lastWakeupEventEpoch;
extern char lastStandbyEventCause[24];
extern char lastWakeupEventCause[24];
extern bool pendingMdsDeviceEventStored;
extern time_t pendingMdsStandbyEventEpoch;
extern time_t pendingMdsWakeupEventEpoch;
extern char pendingMdsStandbyEventCause[24];
extern char pendingMdsWakeupEventCause[24];

String lastMdsStatus = "";

String getLastMdsStatus()
{
  return lastMdsStatus;
}

namespace {
StaticSemaphore_t mdsUploadMutexBuffer;
SemaphoreHandle_t mdsUploadMutex = nullptr;
portMUX_TYPE mdsUploadMutexInitMux = portMUX_INITIALIZER_UNLOCKED;

class MdsUploadGuard {
public:
  MdsUploadGuard()
  {
    if (mdsUploadMutex == nullptr) {
      taskENTER_CRITICAL(&mdsUploadMutexInitMux);
      if (mdsUploadMutex == nullptr) {
        mdsUploadMutex = xSemaphoreCreateMutexStatic(&mdsUploadMutexBuffer);
      }
      taskEXIT_CRITICAL(&mdsUploadMutexInitMux);
    }
    locked = mdsUploadMutex != nullptr && xSemaphoreTake(mdsUploadMutex, 0) == pdTRUE;
  }

  ~MdsUploadGuard()
  {
    if (locked) {
      xSemaphoreGive(mdsUploadMutex);
    }
  }

  bool acquired() const { return locked; }

private:
  bool locked = false;
};

const size_t MAX_WEB_FILE_DOWNLOAD_SIZE = 262144;
const size_t MAX_WEB_BUNDLE_DOWNLOAD_SIZE = 1048576;
const char WEB_BUNDLE_DOWNLOAD_PATH[] = "/webbundle-server.tar";
const char *WEB_INTERFACE_FILES[] = {
  "css_black.css",
  "css_red.css",
  "css_white.css",
  "common.css",
  "common.js",
  "devinfo.html",
  "error.html",
  "favicon.ico",
  "filesystem.html",
  "filesystem.js",
  "firmware.html",
  "firmware-page.js",
  "gauge.min.js",
  "header.html",
  "header.js",
  "app.js",
  "lora.html",
  "index.html",
  "index.js",
  "lora.js",
  "restart.html",
  "restart.js",
  "sensorv.html",
  "sensorv.js",
  "settings-page.css",
  "settings.html",
  "settings.js"
};
const size_t WEB_INTERFACE_FILE_COUNT = sizeof(WEB_INTERFACE_FILES) / sizeof(WEB_INTERFACE_FILES[0]);

bool isStandbyEnabledForMds(const configData &config)
{
  if (String(config.standbyMode) != "On") {
    return false;
  }

  const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  const bool wakeupCycleActive =
      wakeupCause == ESP_SLEEP_WAKEUP_EXT0 ||
      wakeupCause == ESP_SLEEP_WAKEUP_EXT1 ||
      wakeupCause == ESP_SLEEP_WAKEUP_TIMER ||
      wakeupCause == ESP_SLEEP_WAKEUP_TOUCHPAD ||
      wakeupCause == ESP_SLEEP_WAKEUP_ULP;

  const bool sleepWakeupModeActive = !mainPowerOn;
  return sleepWakeupModeActive || wakeupCycleActive || pendingMdsDeviceEventStored;
}

String getStandbyStateForMds(const configData &config)
{
  if (String(config.standbyMode) != "On") {
    return "always_online";
  }

  // The logical input is true when the optocoupler pulls GPIO39 LOW with 12 V present.
  if (mainPowerOn) {
    return "always_online";
  }

  return "wakeup";
}

void addMdsBoardMetadata(JsonObject &board, const configData &config)
{
  board["apiKey"] = String(config.MdsApiKey);
  board["protocolVersion"] = "1";
  board["macAddress"] = WiFi.macAddress();
  board["firmwareVersion"] = String(config.fversion);
  board["standbyEnabled"] = isStandbyEnabledForMds(config);
  board["standbyState"] = getStandbyStateForMds(config);
  board["mainPowerOn"] = mainPowerOn != 0;
}

bool getLittleFsFileSha256(const String &path, String &sha256) {
  sha256 = "";
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path.c_str(), FILE_READ);
  if (!file) {
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  mbedtls_sha256_starts_ret(&shaContext, 0);

  uint8_t buffer[1024];
  while (file.available()) {
    const size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      break;
    }
    mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
  }

  file.close();

  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&shaContext, digest);
  mbedtls_sha256_free(&shaContext);
  sha256 = sha256ToHexString(digest);
  return sha256.length() == 64;
}

bool isWebFileCurrent(const char *fileName, const String &expectedSha256) {
  const String path = "/" + String(fileName);
  String actualSha256;
  if (!getLittleFsFileSha256(path, actualSha256)) {
    return false;
  }
  return actualSha256 == expectedSha256;
}

bool fetchFirmwareManifest(JsonDocument &manifest);
String getExpectedWebFileSha256(JsonDocument &manifest, const char *fversion, const char *fileName);

bool verifyInstalledWebFiles(JsonDocument &manifest, const char *fversion, String &errorMessage) {
  for (size_t i = 0; i < WEB_INTERFACE_FILE_COUNT; i++) {
    const char *fileName = WEB_INTERFACE_FILES[i];
    const String expectedSha256 = getExpectedWebFileSha256(manifest, fversion, fileName);
    if (expectedSha256.length() == 0) {
      errorMessage = "Missing manifest hash for " + String(fileName) + ".";
      return false;
    }

    if (!isWebFileCurrent(fileName, expectedSha256)) {
      errorMessage = "Installed web file failed verification: " + String(fileName) + ".";
      return false;
    }
  }

  return true;
}

String normalizeManifestChannelName(const String &channel) {
  String normalized = channel;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized == "release") {
    return "stable";
  }
  return normalized;
}

JsonObject findManifestReleaseForVersion(JsonDocument &manifest, const char *fversion, String *resolvedChannel = nullptr) {
  if (fversion == nullptr || fversion[0] == '\0') {
    return JsonObject();
  }

  const String preferredChannel = normalizeManifestChannelName(getConfiguredUpdateChannel());
  const char *channels[] = {"stable", "beta"};

  for (const char *channel : channels) {
    if (String(channel) != preferredChannel) {
      continue;
    }
    JsonObject entry = manifest[channel].as<JsonObject>();
    if (!entry.isNull() && String(entry["version"] | "") == String(fversion)) {
      if (resolvedChannel != nullptr) {
        *resolvedChannel = String(channel);
      }
      return entry;
    }
  }

  for (const char *channel : channels) {
    JsonObject entry = manifest[channel].as<JsonObject>();
    if (!entry.isNull() && String(entry["version"] | "") == String(fversion)) {
      if (resolvedChannel != nullptr) {
        *resolvedChannel = String(channel);
      }
      return entry;
    }
  }

  return JsonObject();
}

String getManifestWebFilesBasePath(JsonDocument &manifest, const char *fversion) {
  JsonObject entry = findManifestReleaseForVersion(manifest, fversion);
  if (entry.isNull()) {
    return "";
  }

  String webFilesPath = entry["webFiles"] | "";
  webFilesPath.trim();
  while (webFilesPath.endsWith("/")) {
    webFilesPath.remove(webFilesPath.length() - 1);
  }
  return webFilesPath;
}

bool downloadAndInstallWebBundle(String &errorMessage) {
  JsonDocument manifest;
  const bool manifestAvailable = fetchFirmwareManifest(manifest);
  if (!manifestAvailable) {
    errorMessage = "Manifest is unavailable; using verified single-file download.";
    return false;
  }

  String webFilesBasePath = getManifestWebFilesBasePath(manifest, actconf.fversion);
  if (webFilesBasePath.length() == 0) {
    errorMessage = "Manifest has no matching web files entry; using verified single-file download.";
    return false;
  }

  JsonObject release = findManifestReleaseForVersion(manifest, actconf.fversion);
  const String expectedBundleSha256 = normalizeSha256String(release["webBundleSha256"] | "");
  if (expectedBundleSha256.length() == 0) {
    errorMessage = "Manifest has no web package checksum; using verified single-file download.";
    return false;
  }

  const String baseUrl = buildMdsOtaWebBaseUrl(String(actconf.mdsOtaUrl));
  if (baseUrl.length() == 0) {
    errorMessage = "MDS OTA endpoint is not configured.";
    return false;
  }

  const String bundleUrl = baseUrl + "/" + webFilesBasePath + "/webui-package.tar";
  WiFiClientSecure client;
  HTTPClient http;

  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  if (!http.begin(client, bundleUrl)) {
    errorMessage = "Could not initialize web bundle download.";
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    errorMessage = "Web bundle download failed with HTTP " + String(httpCode) + ".";
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0 || static_cast<size_t>(contentLength) > MAX_WEB_BUNDLE_DOWNLOAD_SIZE) {
    errorMessage = "Web bundle has an invalid size.";
    http.end();
    return false;
  }
  setWebFilesUpdateProgress(0, static_cast<size_t>(contentLength), "webui-package.tar", "Downloading web package.");

  if (LittleFS.exists(WEB_BUNDLE_DOWNLOAD_PATH)) {
    LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
  }

  File targetFile = LittleFS.open(WEB_BUNDLE_DOWNLOAD_PATH, FILE_WRITE);
  if (!targetFile) {
    errorMessage = "Could not create temporary web bundle file.";
    http.end();
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  mbedtls_sha256_starts_ret(&shaContext, 0);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  unsigned long lastProgressMillis = millis();
  int lastDisplayProgressBucket = -1;
  while (http.connected() && written < static_cast<size_t>(contentLength)) {
    const size_t availableBytes = stream->available();
    if (availableBytes == 0) {
      if (millis() - lastProgressMillis > 15000UL) {
        targetFile.close();
        mbedtls_sha256_free(&shaContext);
        LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
        http.end();
        errorMessage = "Web bundle download timed out.";
        return false;
      }
      delay(1);
      continue;
    }

    const size_t remainingBytes = static_cast<size_t>(contentLength) - written;
    const size_t toRead = min(sizeof(buffer), min(availableBytes, remainingBytes));
    const size_t bytesRead = stream->readBytes(buffer, toRead);
    if (bytesRead == 0) {
      delay(1);
      continue;
    }

    if (targetFile.write(buffer, bytesRead) != bytesRead) {
      targetFile.close();
      mbedtls_sha256_free(&shaContext);
      LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
      http.end();
      errorMessage = "Could not write downloaded web bundle.";
      return false;
    }
    written += bytesRead;
    mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
    lastProgressMillis = millis();
    setWebFilesUpdateProgress(written, static_cast<size_t>(contentLength), "webui-package.tar", "Downloading web package.");
    const int displayPercent = int((written * 100U) / static_cast<size_t>(contentLength));
    const int displayBucket = displayPercent / 5;
    if (displayBucket != lastDisplayProgressBucket || written == static_cast<size_t>(contentLength)) {
      lastDisplayProgressBucket = displayBucket;
      writeDisplayProgressScreen("Web files",
                                 "Downloading bundle",
                                 written,
                                 static_cast<size_t>(contentLength),
                                 "webui-package.tar");
    }
  }

  targetFile.close();
  http.end();

  if (written != static_cast<size_t>(contentLength)) {
    mbedtls_sha256_free(&shaContext);
    LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
    errorMessage = "Downloaded web bundle is incomplete.";
    return false;
  }

  uint8_t bundleDigest[32];
  mbedtls_sha256_finish_ret(&shaContext, bundleDigest);
  mbedtls_sha256_free(&shaContext);
  if (sha256ToHexString(bundleDigest) != expectedBundleSha256) {
    LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
    errorMessage = "Downloaded web package checksum mismatch.";
    return false;
  }

  String installedVersion;
  const bool success = installWebBundleFromTar(WEB_BUNDLE_DOWNLOAD_PATH, installedVersion, errorMessage);
  LittleFS.remove(WEB_BUNDLE_DOWNLOAD_PATH);
  if (!success) {
    return false;
  }

  String verificationError;
  if (!verifyInstalledWebFiles(manifest, actconf.fversion, verificationError)) {
    errorMessage = verificationError;
    return false;
  }

  setWebFilesUpdateProgress(1, 1, "", "Web package installed successfully.");
  return true;
}

bool fetchTextFromUpdateServer(const String &url, String &payload) {
  payload = "";

  if (WiFi.status() != WL_CONNECTED) {
    DebugPrintln(1, "Manifest request skipped because WiFi is not connected");
    return false;
  }

  const unsigned long waitStart = millis();
  while (!hasReasonableSystemTime() && millis() - waitStart < 4000UL) {
    delay(100);
  }

  if (!hasReasonableSystemTime()) {
    DebugPrintln(1, "Manifest request skipped because system time is not synchronized");
    return false;
  }

  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    http.setReuse(false);
    http.addHeader("Cache-Control", "no-cache");
    http.addHeader("Pragma", "no-cache");

    if (!http.begin(client, url)) {
      DebugPrintln(1, "- failed to initialize HTTPS manifest request");
    } else {
      const int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        payload.trim();
        http.end();
        if (payload.length() > 0) {
          return true;
        }
      } else {
        DebugPrint(1, "Manifest request failed: ");
        DebugPrintln(1, httpCode);
      }
      http.end();
    }

    if (attempt == 0) {
      delay(250);
    }
  }

  return false;
}

bool fetchFirmwareManifest(JsonDocument &manifest) {
  const String baseUrl = buildMdsOtaWebBaseUrl(String(actconf.mdsOtaUrl));
  if (baseUrl.length() == 0) {
    DebugPrintln(1, "MDS OTA endpoint is not configured");
    return false;
  }
  const String preferredChannel = normalizeManifestChannelName(getConfiguredUpdateChannel());
  const String candidateUrls[] = {
    baseUrl + "/" + preferredChannel + ".json",
    baseUrl + "/firmware-manifest.json"
  };

  String manifestPayload;
  bool fetched = false;
  for (size_t i = 0; i < (sizeof(candidateUrls) / sizeof(candidateUrls[0])); ++i) {
    manifestPayload = "";
    if (!fetchTextFromUpdateServer(candidateUrls[i], manifestPayload)) {
      continue;
    }
    fetched = true;
    break;
  }

  if (!fetched) {
    return false;
  }

  DeserializationError error = deserializeJson(manifest, manifestPayload);
  if (error) {
    DebugPrint(1, "Firmware manifest JSON error: ");
    DebugPrintln(1, error.c_str());
    return false;
  }

  if (!manifest[preferredChannel.c_str()].isNull() || !manifest["version"].isNull()) {
    if (!manifest["version"].isNull()) {
      JsonDocument wrapped;
      wrapped[preferredChannel] = manifest.as<JsonObject>();
      manifest.clear();
      manifest.set(wrapped);
    }
    return true;
  }

  DebugPrintln(1, "Firmware metadata JSON does not contain a usable release entry");
  return false;
}

String getExpectedWebFileSha256(JsonDocument &manifest, const char *fversion, const char *fileName) {
  JsonObject entry = findManifestReleaseForVersion(manifest, fversion);
  if (entry.isNull()) {
    return "";
  }
  const char *sha256 = entry["webFileHashes"][fileName] | "";
  return normalizeSha256String(String(sha256));
}

bool hasManifestReleaseForVersion(JsonDocument &manifest, const char *fversion) {
  return !findManifestReleaseForVersion(manifest, fversion).isNull();
}

String normalizeSecureUrl(const String &configuredValue)
{
  String normalized = configuredValue;
  normalized.trim();

  if (normalized.startsWith("http://")) {
    normalized.remove(0, 7);
    normalized = "https://" + normalized;
  }

  if (normalized.startsWith("https://git.derguntmar.de/")) {
    normalized.replace("https://git.derguntmar.de/", "https://mds-git.derguntmar.de/");
  }
  if (normalized.startsWith("https://s-git.derguntmar.de/")) {
    normalized.replace("https://s-git.derguntmar.de/", "https://mds-git.derguntmar.de/");
  }

  return normalized;
}

bool hasValidSystemTime()
{
  return hasReasonableSystemTime();
}

bool waitForMdsSystemTime(uint32_t timeoutMs)
{
  const unsigned long start = millis();
  while (!hasValidSystemTime() && (millis() - start < timeoutMs)) {
    delay(250);
  }
  return hasValidSystemTime();
}

bool ensureMdsSystemTime()
{
  if (hasValidSystemTime()) {
    return true;
  }

  DebugPrintln(2, "System time is not synchronized. Retrying NTP before MDS request...");
  configTime(3600, 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  if (waitForMdsSystemTime(12000)) {
    struct tm tmstruct;
    if (getLocalTime(&tmstruct, 1000)) {
      DebugPrintln(3, "\nMDS time sync OK: " + String((tmstruct.tm_year) + 1900) + "-" + String((tmstruct.tm_mon) + 1) + "-" + String(tmstruct.tm_mday) + " " + String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));
    }
    return true;
  }

  return false;
}

bool parseHttpStatusCode(const String &statusLine, int &httpResponseCode)
{
  httpResponseCode = -1;
  String trimmedStatusLine = statusLine;
  trimmedStatusLine.trim();
  const int firstSpace = trimmedStatusLine.indexOf(' ');
  if (firstSpace < 0) {
    return false;
  }

  const int secondSpace = trimmedStatusLine.indexOf(' ', firstSpace + 1);
  const String codeToken = secondSpace >= 0
    ? trimmedStatusLine.substring(firstSpace + 1, secondSpace)
    : trimmedStatusLine.substring(firstSpace + 1);
  httpResponseCode = codeToken.toInt();
  return httpResponseCode > 0;
}

void buildMdsTimestamp(char (&mdsDate)[11], char (&mdsTime)[9])
{
  struct tm tmstruct;
  if (getLocalTime(&tmstruct, 1000)) {
    strftime(mdsDate, sizeof(mdsDate), "%d.%m.%Y", &tmstruct);
    strftime(mdsTime, sizeof(mdsTime), "%H:%M:%S", &tmstruct);
    return;
  }

  strcpy(mdsDate, "01.01.1970");
  strcpy(mdsTime, "00:00:00");
}

String formatMdsDateTime(time_t timestamp)
{
  if (timestamp <= 0) {
    return "";
  }

  struct tm tmstruct;
  if (!localtime_r(&timestamp, &tmstruct)) {
    return "";
  }

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &tmstruct);
  return String(buffer);
}

void addMdsSensorRecord(JsonArray &sensors, int enabledMarker, const char *sensorType, const char *sensorName,
                        const char *dateValue, const char *timeValue, const char *transmissionPath,
                        float value1, float value2, float value3, float value4)
{
  if (enabledMarker <= 0 || sensorType == nullptr || sensorType[0] == '\0') {
    return;
  }

  JsonObject sensor = sensors.add<JsonObject>();
  sensor["sensorType"] = sensorType;
  sensor["type"] = sensorType;

  if (sensorName != nullptr && sensorName[0] != '\0') {
    sensor["sensorName"] = sensorName;
    sensor["name"] = sensorName;
  }

  sensor["value1"] = value1;
  sensor["value2"] = value2;
  sensor["value3"] = value3;
  sensor["value4"] = value4;
  sensor["date"] = dateValue;
  sensor["time"] = timeValue;
  sensor["transmissionPath"] = transmissionPath;
}

bool postMdsPayload(const configData &actconf, JsonDocument &docpayload, const char *contextLabel)
{
  MdsUploadGuard uploadGuard;
  if (!uploadGuard.acquired()) {
    DebugPrintln(2, String("Skipping overlapping MDS request (") + contextLabel + ")");
    return false;
  }

  lastMdsStatus = "";
  const String mdsUrl = normalizeSecureUrl(String(actconf.MdsUrl));
  if (mdsUrl.length() == 0) {
    lastMdsStatus = "MDS URL missing";
    DebugPrintln(1, "MDS URL missing, skipping WiFi upload");
    return false;
  }

  if (!mdsUrl.startsWith("https://")) {
    lastMdsStatus = "Refusing insecure MDS URL. Please use https://";
    DebugPrintln(1, "Refusing insecure MDS URL. Please use https://");
    return false;
  }

  if (!ensureMdsSystemTime()) {
    lastMdsStatus = "System time is not synchronized";
    DebugPrintln(2, "Skipping MDS request because system time is not synchronized yet");
    return false;
  }

  String requestBody;
  serializeJson(docpayload, requestBody);
  String debugRequestBody;
  if (actconf.debug >= 3) {
    JsonDocument debugPayload;
    deserializeJson(debugPayload, requestBody);
    if (debugPayload["board"]["apiKey"].is<String>()) {
      debugPayload["board"]["apiKey"] = "***hidden***";
    }
    serializeJson(debugPayload, debugRequestBody);
  } else {
    debugRequestBody = "***hidden***";
  }

  auto handleMdsResponse = [&](int httpResponseCode, const String &responseBody) -> bool {
    DebugPrint(3, "HTTP response: ");
    DebugPrintln(3, httpResponseCode);
    if (responseBody.length() > 0) {
      DebugPrintln(3, responseBody);
      lastMdsStatus = responseBody;
    }

    bool success = httpResponseCode >= 200 && httpResponseCode < 300;
    if (success) {
      JsonDocument responseJson;
      if (deserializeJson(responseJson, responseBody) == DeserializationError::Ok) {
        const String responseStatus = responseJson["status"] | "";
        if (responseStatus.length() > 0) {
          success = responseStatus == "ok";
        }
        if (responseJson["insertedSensorRows"].is<int>() || responseJson["skippedSensorRows"].is<int>()) {
          const int insertedSensorRows = responseJson["insertedSensorRows"] | 0;
          const int skippedSensorRows = responseJson["skippedSensorRows"] | 0;
          DebugPrintln(3, String("MDS inserted rows: ") + insertedSensorRows);
          DebugPrintln(3, String("MDS skipped rows: ") + skippedSensorRows);
          DebugPrintln(3, String("MDS auto resolved rows: ") + int(responseJson["autoResolvedSensorRows"] | 0));
          DebugPrintln(3, String("MDS auto created sensor configs: ") + int(responseJson["autoCreatedSensorConfigs"] | 0));
          if (insertedSensorRows <= 0 || skippedSensorRows > 0) {
            success = false;
            lastMdsStatus = "MDS did not store every sensor row";
            DebugPrintln(2, "MDS response did not confirm complete sensor storage");
          } else {
            lastMdsStatus = "MDS upload accepted";
          }
        }
      }
    }
    return success;
  };

  WiFiClientSecure client;
  client.setHandshakeTimeout(15);
  client.setTimeout(15);

  DebugPrintln(3, String("Starting MDS request (") + contextLabel + ")");
  DebugPrintln(3, "MDS URL: " + mdsUrl);

  String urlWithoutScheme = mdsUrl;
  urlWithoutScheme.remove(0, 8);
  const int pathStart = urlWithoutScheme.indexOf('/');
  const String host = pathStart >= 0 ? urlWithoutScheme.substring(0, pathStart) : urlWithoutScheme;
  const String path = pathStart >= 0 ? urlWithoutScheme.substring(pathStart) : "/";
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));

  IPAddress resolvedIp;
  if (!WiFi.hostByName(host.c_str(), resolvedIp)) {
    lastMdsStatus = "DNS lookup failed for MDS host: " + host;
    DebugPrintln(1, "DNS lookup failed for MDS host: " + host);
    return false;
  }
  DebugPrintln(3, "MDS host resolved to: " + resolvedIp.toString());

  DebugPrint(3, "HTTP POST payload: ");
  DebugPrintln(3, debugRequestBody.length() > 0 ? debugRequestBody : String("***hidden***"));

  if (!client.connect(host.c_str(), 443)) {
    char errorBuffer[128] = {0};
    client.lastError(errorBuffer, sizeof(errorBuffer));
    client.stop();
    lastMdsStatus = "Failed to connect to MDS host";
    if (errorBuffer[0] != '\0') {
      lastMdsStatus += ": " + String(errorBuffer);
    }
    DebugPrintln(1, "Failed to connect to MDS host: " + host + " (" + resolvedIp.toString() + ")");
    if (errorBuffer[0] != '\0') {
      DebugPrintln(1, "TLS client error: " + String(errorBuffer));
    }
    return false;
  }

  client.print(String("POST ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("User-Agent: LoRaBoatMonitor/ESP32\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print(String("Content-Length: ") + requestBody.length() + "\r\n\r\n");
  client.print(requestBody);

  const String statusLine = client.readStringUntil('\n');
  int httpResponseCode = -1;
  String trimmedStatusLine = statusLine;
  trimmedStatusLine.trim();
  DebugPrintln(3, "HTTP status line: " + trimmedStatusLine);
  parseHttpStatusCode(trimmedStatusLine, httpResponseCode);

  while (client.connected()) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r" || headerLine.length() == 0) {
      break;
    }
  }

  const String responseBody = client.readString();
  client.stop();
  if (httpResponseCode > 0) {
    return handleMdsResponse(httpResponseCode, responseBody);
  }

  DebugPrintln(1, "Primary MDS upload path failed before a valid HTTP response was received");
  DebugPrintln(2, "Trying HTTPClient fallback for MDS upload");

  WiFiClientSecure fallbackClient;
  HTTPClient http;
  fallbackClient.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setReuse(false);

  if (!http.begin(fallbackClient, mdsUrl)) {
    DebugPrintln(1, "Failed to initialize HTTPClient fallback for MDS upload");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.addHeader("User-Agent", "LoRaBoatMonitor/ESP32");
  const int fallbackResponseCode = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(requestBody.c_str())), requestBody.length());
  const String fallbackResponseBody = http.getString();
  if (fallbackResponseCode <= 0) {
    DebugPrintln(1, "WiFi upload failed: " + http.errorToString(fallbackResponseCode));
    http.end();
    return false;
  }

  const bool success = handleMdsResponse(fallbackResponseCode, fallbackResponseBody);
  http.end();
  return success;
}
}

namespace {
const char WEB_FILES_TRANSACTION_JOURNAL[] = "/webfiles-update.journal";

struct PendingWebFileUpdate {
  String targetPath;
  String tempPath;
  String backupPath;
  bool hadOriginal = false;
};

bool downloadWebFileToPath(const char *fileName,
                           const char *webFilesBasePath,
                           const String &expectedSha256,
                           const String &destinationPath);

void cleanupPendingWebFiles(PendingWebFileUpdate *updates, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    LittleFS.remove(updates[i].tempPath);
    LittleFS.remove(updates[i].backupPath);
  }
}

void rollbackPendingWebFiles(PendingWebFileUpdate *updates, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (LittleFS.exists(updates[i].backupPath)) {
      LittleFS.remove(updates[i].targetPath);
      LittleFS.rename(updates[i].backupPath, updates[i].targetPath);
    } else if (!updates[i].hadOriginal) {
      LittleFS.remove(updates[i].targetPath);
    }
    LittleFS.remove(updates[i].tempPath);
  }
  LittleFS.remove(WEB_FILES_TRANSACTION_JOURNAL);
}

bool writeWebFilesTransactionJournal(PendingWebFileUpdate *updates, size_t count) {
  LittleFS.remove(WEB_FILES_TRANSACTION_JOURNAL);
  File journal = LittleFS.open(WEB_FILES_TRANSACTION_JOURNAL, FILE_WRITE);
  if (!journal) return false;

  for (size_t i = 0; i < count; ++i) {
    const String line = String(updates[i].hadOriginal ? "1" : "0") + "|" +
      updates[i].targetPath + "|" + updates[i].backupPath + "|" + updates[i].tempPath + "\n";
    if (journal.print(line) != line.length()) {
      journal.close();
      LittleFS.remove(WEB_FILES_TRANSACTION_JOURNAL);
      return false;
    }
  }
  journal.close();
  return true;
}

bool commitPendingWebFiles(PendingWebFileUpdate *updates, size_t count, String &errorMessage) {
  for (size_t i = 0; i < count; ++i) {
    updates[i].hadOriginal = LittleFS.exists(updates[i].targetPath);
  }
  if (!writeWebFilesTransactionJournal(updates, count)) {
    errorMessage = "Could not create web files transaction journal.";
    cleanupPendingWebFiles(updates, count);
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    LittleFS.remove(updates[i].backupPath);
    if (updates[i].hadOriginal && !LittleFS.rename(updates[i].targetPath, updates[i].backupPath)) {
      errorMessage = "Could not back up " + updates[i].targetPath + ".";
      rollbackPendingWebFiles(updates, count);
      return false;
    }
    if (!LittleFS.rename(updates[i].tempPath, updates[i].targetPath)) {
      errorMessage = "Could not install " + updates[i].targetPath + ".";
      rollbackPendingWebFiles(updates, count);
      return false;
    }
  }

  // Removing the journal marks the complete set as committed. Backups can then
  // be discarded without risking a mixed old/new web interface after reboot.
  LittleFS.remove(WEB_FILES_TRANSACTION_JOURNAL);
  for (size_t i = 0; i < count; ++i) {
    LittleFS.remove(updates[i].backupPath);
  }
  return true;
}
}  // namespace

void recoverInterruptedWebFilesUpdate() {
  const bool transactionWasInterrupted = LittleFS.exists(WEB_FILES_TRANSACTION_JOURNAL);
  if (transactionWasInterrupted) {
    PendingWebFileUpdate recovered[WEB_INTERFACE_FILE_COUNT];
    size_t recoveredCount = 0;
    File journal = LittleFS.open(WEB_FILES_TRANSACTION_JOURNAL, FILE_READ);
    while (journal && journal.available() && recoveredCount < WEB_INTERFACE_FILE_COUNT) {
      String line = journal.readStringUntil('\n');
      line.trim();
      const int first = line.indexOf('|');
      if (first <= 0) continue;
      const int second = line.indexOf('|', first + 1);
      if (second <= first) continue;
      const int third = line.indexOf('|', second + 1);
      if (third <= second) continue;

      PendingWebFileUpdate &entry = recovered[recoveredCount++];
      entry.hadOriginal = line.substring(0, first) == "1";
      entry.targetPath = line.substring(first + 1, second);
      entry.backupPath = line.substring(second + 1, third);
      entry.tempPath = line.substring(third + 1);
    }
    if (journal) journal.close();
    rollbackPendingWebFiles(recovered, recoveredCount);
    DebugPrintln(2, "Recovered interrupted web files transaction.");
  }

  for (size_t i = 0; i < WEB_INTERFACE_FILE_COUNT; ++i) {
    const String name = String(WEB_INTERFACE_FILES[i]);
    const String targetPath = "/" + name;
    const String tempPath = "/." + name + ".download";
    const String backupPath = "/." + name + ".previous";
    LittleFS.remove(tempPath);
    if (transactionWasInterrupted && LittleFS.exists(backupPath)) {
      LittleFS.remove(targetPath);
      LittleFS.rename(backupPath, targetPath);
    } else {
      LittleFS.remove(backupPath);
    }
  }
  LittleFS.remove(WEB_FILES_TRANSACTION_JOURNAL);
}

bool DownloadFilesFromWeb()
{
  const char *fversion = actconf.fversion;
  DebugPrintln(3, "Downloading web files for installed firmware version: " + String(fversion));
  resetWebFilesUpdateState(1, "webui-package.tar", "Downloading web package.");

  String bundleError;
  writeDisplayProgressScreen("Web files", "Downloading bundle", 0, 1, "webui-package.tar");
  if (downloadAndInstallWebBundle(bundleError)) {
    setWebFilesServerSupport(true, true);
    saveWebFilesVersion(fversion);
    setWebFilesUpdateMessage("Web files updated successfully.");
    writeDisplayStatusScreen("Web files", "Update complete", String(fversion));
    return true;
  }

  DebugPrintln(1, "Web bundle install failed, falling back to single-file download: " + bundleError);
  setWebFilesUpdateProgress(0, 0, "", "Bundle install failed: " + bundleError + " Trying single files.");

  JsonDocument manifest;
  if (!fetchFirmwareManifest(manifest)) {
    setWebFilesServerSupport(false, false);
    DebugPrintln(1, "Unable to fetch firmware manifest for web file hash validation");
    setWebFilesUpdateError(true, "Unable to fetch firmware manifest.");
    writeDisplayStatusScreen("Web files", "Manifest failed", "Check WiFi");
    return false;
  }

  if (!hasManifestReleaseForVersion(manifest, fversion)) {
    setWebFilesServerSupport(true, false);
    const String message = "Update server has no web files for " + String(fversion) + ". Use the local web package instead.";
    setWebFilesUpdateFatalError(true, message);
    DebugPrintln(1, message);
    writeDisplayStatusScreen("Web files", "No server files", String(fversion), "Use local pkg");
    return false;
  }

  const String webFilesBasePath = getManifestWebFilesBasePath(manifest, fversion);
  if (webFilesBasePath.length() == 0) {
    setWebFilesServerSupport(true, false);
    setWebFilesUpdateFatalError(true, "Manifest is missing the web files path for " + String(fversion) + ".");
    writeDisplayStatusScreen("Web files", "Manifest error", String(fversion), "Missing path");
    return false;
  }

  setWebFilesServerSupport(true, true);

  setWebFilesUpdateProgress(0, WEB_INTERFACE_FILE_COUNT, "", "Checking web files.");

  bool allFilesUpdated = true;
  PendingWebFileUpdate pendingUpdates[WEB_INTERFACE_FILE_COUNT];
  size_t pendingUpdateCount = 0;
  String currentName;
  for (size_t i = 0; i < WEB_INTERFACE_FILE_COUNT; ++i) {
    currentName = String(WEB_INTERFACE_FILES[i]);
    setWebFilesUpdateProgress(i, WEB_INTERFACE_FILE_COUNT, currentName, "Checking " + currentName + ".");
    writeDisplayProgressScreen("Web files",
                               "Checking file",
                               i,
                               WEB_INTERFACE_FILE_COUNT,
                               currentName);
    const String expectedSha256 = getExpectedWebFileSha256(manifest, fversion, WEB_INTERFACE_FILES[i]);
    if (expectedSha256.length() == 0) {
      DebugPrint(1, "Missing manifest hash for web file: ");
      DebugPrintln(1, WEB_INTERFACE_FILES[i]);
      setWebFilesUpdateFatalError(true, "Update server is incomplete for " + String(fversion) + ". Missing hash for " + currentName + ".");
      allFilesUpdated = false;
      break;
    } else {
      if (isWebFileCurrent(WEB_INTERFACE_FILES[i], expectedSha256)) {
        DebugPrint(3, "Web file already current, skipping download: ");
        DebugPrintln(3, WEB_INTERFACE_FILES[i]);
      } else {
        setWebFilesUpdateProgress(i, WEB_INTERFACE_FILE_COUNT, currentName, "Downloading " + currentName + ".");
        writeDisplayProgressScreen("Web files",
                                   "Downloading",
                                   i,
                                   WEB_INTERFACE_FILE_COUNT,
                                   currentName);
        PendingWebFileUpdate &pending = pendingUpdates[pendingUpdateCount];
        pending.targetPath = "/" + currentName;
        pending.tempPath = "/." + currentName + ".download";
        pending.backupPath = "/." + currentName + ".previous";
        LittleFS.remove(pending.tempPath);
        LittleFS.remove(pending.backupPath);
        if (!downloadWebFileToPath(WEB_INTERFACE_FILES[i], webFilesBasePath.c_str(), expectedSha256, pending.tempPath)) {
          allFilesUpdated = false;
          break;
        }
        pendingUpdateCount++;
      }
    }
    setWebFilesUpdateProgress(i + 1, WEB_INTERFACE_FILE_COUNT, currentName, allFilesUpdated ? "File complete." : "File error.");
    writeDisplayProgressScreen("Web files",
                               allFilesUpdated ? "File complete" : "File error",
                               i + 1,
                               WEB_INTERFACE_FILE_COUNT,
                               currentName);
  }

  if (allFilesUpdated && pendingUpdateCount > 0) {
    String transactionError;
    allFilesUpdated = commitPendingWebFiles(pendingUpdates, pendingUpdateCount, transactionError);
    if (!allFilesUpdated) {
      setWebFilesUpdateFatalError(true, transactionError);
      currentName = "transaction";
    }
  } else if (!allFilesUpdated) {
    cleanupPendingWebFiles(pendingUpdates, pendingUpdateCount);
  }

  if (allFilesUpdated) {
    saveWebFilesVersion(fversion);
    setWebFilesUpdateProgress(WEB_INTERFACE_FILE_COUNT, WEB_INTERFACE_FILE_COUNT, "", "Web files updated successfully.");
    writeDisplayStatusScreen("Web files", "Update complete", String(fversion));
  } else {
    writeDisplayStatusScreen("Web files", "Update failed", "See WebSerial", currentName);
  }

  return allFilesUpdated;
}

namespace {
bool downloadWebFileToPath(const char *fileName,
                           const char *webFilesBasePath,
                           const String &expectedSha256,
                           const String &destinationPath)
{
  const String normalizedExpectedSha256 = normalizeSha256String(expectedSha256);
  if (normalizedExpectedSha256.length() == 0) {
    DebugPrint(1, "Refusing web file download without valid SHA256: ");
    DebugPrintln(1, fileName);
    return false;
  }

  const String baseUrl = buildMdsOtaWebBaseUrl(String(actconf.mdsOtaUrl));
  String normalizedWebFilesBasePath = String(webFilesBasePath == nullptr ? "" : webFilesBasePath);
  normalizedWebFilesBasePath.trim();
  while (normalizedWebFilesBasePath.startsWith("/")) {
    normalizedWebFilesBasePath.remove(0, 1);
  }
  while (normalizedWebFilesBasePath.endsWith("/")) {
    normalizedWebFilesBasePath.remove(normalizedWebFilesBasePath.length() - 1);
  }
  const String myurl = baseUrl + "/" + normalizedWebFilesBasePath + "/" + String(fileName);
  WiFiClientSecure client;
  HTTPClient http;

  DebugPrint(3, "Connecting to website: ");
  DebugPrintln(3, baseUrl);
  DebugPrintln(3, myurl);

  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, myurl)) {
    DebugPrintln(1, "- failed to initialize HTTPS request");
    return false;
  }

  Serial.print("[HTTP] GET...\n");
  const int httpCode = http.GET();
  bool downloadSuccess = false;

  if(httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    if(httpCode == HTTP_CODE_OK) {
      const int contentLength = http.getSize();
      if (contentLength < 0 || static_cast<size_t>(contentLength) > MAX_WEB_FILE_DOWNLOAD_SIZE) {
        DebugPrintln(1, "- invalid web file size");
        http.end();
        return false;
      }

      if (LittleFS.exists(destinationPath)) {
        LittleFS.remove(destinationPath);
      }

      File targetFile = LittleFS.open(destinationPath.c_str(), FILE_WRITE);
      if (!targetFile) {
        DebugPrintln(1, "- failed to open file for writing");
        http.end();
        return false;
      }

      WiFiClient *stream = http.getStreamPtr();
      uint8_t buffer[1024];
      size_t written = 0;
      unsigned long lastProgressMillis = millis();
      mbedtls_sha256_context shaContext;
      mbedtls_sha256_init(&shaContext);
      mbedtls_sha256_starts_ret(&shaContext, 0);

      while (http.connected() && written < static_cast<size_t>(contentLength)) {
        const size_t availableBytes = stream->available();
        if (availableBytes == 0) {
          if (millis() - lastProgressMillis > 15000UL) {
            DebugPrintln(1, "- web file download timed out");
            targetFile.close();
            mbedtls_sha256_free(&shaContext);
            LittleFS.remove(destinationPath);
            http.end();
            return false;
          }
          delay(1);
          continue;
        }

        const size_t remainingBytes = static_cast<size_t>(contentLength) - written;
        const size_t toRead = min(sizeof(buffer), min(availableBytes, remainingBytes));
        const size_t bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead == 0) {
          delay(1);
          continue;
        }

        if (targetFile.write(buffer, bytesRead) != bytesRead) {
          DebugPrintln(1, "- failed to write downloaded file");
          targetFile.close();
          mbedtls_sha256_free(&shaContext);
          LittleFS.remove(destinationPath);
          http.end();
          return false;
        }

        mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
        written += bytesRead;
        lastProgressMillis = millis();
      }

      targetFile.close();

      uint8_t digest[32];
      mbedtls_sha256_finish_ret(&shaContext, digest);
      mbedtls_sha256_free(&shaContext);

      if (written != static_cast<size_t>(contentLength)) {
        DebugPrintln(1, "- incomplete web file download");
        LittleFS.remove(destinationPath);
        http.end();
        return false;
      }

      const String actualSha256 = sha256ToHexString(digest);
      if (actualSha256 != normalizedExpectedSha256) {
        DebugPrint(1, "Web file checksum mismatch: ");
        DebugPrintln(1, fileName);
        LittleFS.remove(destinationPath);
        http.end();
        return false;
      }

      downloadSuccess = true;
    } else if (httpCode == HTTP_CODE_NOT_FOUND) {
      DebugPrintln(1, "- 404 page not found");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  DebugPrintln(3, "end.");
  return downloadSuccess;
}
}  // namespace

bool sendToMDS(const configData &actconf)
{
  if (WiFi.status() != WL_CONNECTED) {
    lastMdsStatus = "WiFi is not connected";
    DebugPrintln(2, "Skipping MDS sensor upload because WiFi is not connected");
    return false;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  addMdsBoardMetadata(board, actconf);

  char mdsDate[11];
  char mdsTime[9];
  buildMdsTimestamp(mdsDate, mdsTime);

  const char* transmissionPath = "1";
  addMdsSensorRecord(sensors, actconf.MdsSensorIdBattery, "ADC", "Battery", mdsDate, mdsTime, transmissionPath, voltage, capacity, 0, 0);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdTanks, "ADC", "Tanks", mdsDate, mdsTime, transmissionPath, tank1p, tank1adc, tank2p, tank2adc);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdStatus, "Digital", "Status", mdsDate, mdsTime, transmissionPath, mainPowerOn, actconf.relay, temp1wire, 0);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdGps, "GPS", "GPS", mdsDate, mdsTime, transmissionPath, latitude, longitude, gpsspeed, course);

  if (String(actconf.envSensor) == "BME280") {
    addMdsSensorRecord(sensors, actconf.MdsSensorIdEnv, "BME280", "Environment", mdsDate, mdsTime, transmissionPath, temperature, humidity, pressure, altitude);
    addMdsSensorRecord(sensors, actconf.MdsSensorIdDewpoint, "BME280", "Dewpoint", mdsDate, mdsTime, transmissionPath, dewp, 0, 0, 0);
  }

  if (String(actconf.envSensor) == "VEdirect-Read") {
    addMdsSensorRecord(sensors, actconf.MdsSensorIdVedirect, "DS2438", "VEdirect", mdsDate, mdsTime, transmissionPath, vedirectVoltage, vedirectCurrent, vedirectTemp, 0);
  }

  if (sensors.size() == 0) {
    lastMdsStatus = "No MDS sensor groups are enabled";
    DebugPrintln(2, "Skipping MDS sensor upload because no MDS uploads are enabled");
    return false;
  }

  return postMdsPayload(actconf, docpayload, "sensor upload");
}

bool sendMdsDeviceEvent(const configData &actconf, const char *sensorName)
{
  if (WiFi.status() != WL_CONNECTED) {
    lastMdsStatus = "WiFi is not connected";
    DebugPrintln(2, String("Skipping MDS device event because WiFi is not connected: ") + String(sensorName == nullptr ? "" : sensorName));
    return false;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  addMdsBoardMetadata(board, actconf);

  char mdsDate[11];
  char mdsTime[9];
  buildMdsTimestamp(mdsDate, mdsTime);

  JsonObject sensor = sensors.add<JsonObject>();
  sensor["sensorType"] = "WakeupStan";
  sensor["type"] = "WakeupStan";
  sensor["sensorName"] = "WakeupLog";
  sensor["name"] = "WakeupLog";
  sensor["value1"] = String(pendingMdsDeviceEventStored ? pendingMdsStandbyEventCause : lastStandbyEventCause);
  sensor["value2"] = formatMdsDateTime(pendingMdsDeviceEventStored ? pendingMdsStandbyEventEpoch : lastStandbyEventEpoch);
  sensor["value3"] = String(pendingMdsDeviceEventStored ? pendingMdsWakeupEventCause : lastWakeupEventCause);
  sensor["value4"] = formatMdsDateTime(pendingMdsDeviceEventStored ? pendingMdsWakeupEventEpoch : lastWakeupEventEpoch);
  sensor["date"] = mdsDate;
  sensor["time"] = mdsTime;
  sensor["transmissionPath"] = "1";

  return postMdsPayload(actconf, docpayload, sensorName == nullptr ? "device event" : sensorName);
}

bool sendMdsOtaStatus(const configData &actconf, const char *phase, int percent, const String &targetVersion, const String &message)
{
  if (WiFi.status() != WL_CONNECTED) {
    DebugPrintln(2, "Skipping MDS OTA status because WiFi is not connected");
    return false;
  }

  if (String(actconf.MdsApiKey).length() == 0) {
    DebugPrintln(2, "Skipping MDS OTA status because MDS API key is not configured");
    return false;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  addMdsBoardMetadata(board, actconf);

  char mdsDate[11];
  char mdsTime[9];
  buildMdsTimestamp(mdsDate, mdsTime);

  String clippedMessage = message;
  clippedMessage.replace("\r", " ");
  clippedMessage.replace("\n", " ");
  if (clippedMessage.length() > 140) {
    clippedMessage = clippedMessage.substring(0, 137) + "...";
  }

  JsonObject sensor = sensors.add<JsonObject>();
  sensor["sensorType"] = "OtaStatus";
  sensor["type"] = "OtaStatus";
  sensor["sensorName"] = "OtaUpdate";
  sensor["name"] = "OtaUpdate";
  sensor["value1"] = String(phase == nullptr ? "" : phase);
  sensor["value2"] = percent < 0 ? 0 : percent;
  sensor["value3"] = targetVersion.length() > 0 ? targetVersion : String(actconf.fversion);
  sensor["value4"] = clippedMessage;
  sensor["date"] = mdsDate;
  sensor["time"] = mdsTime;
  sensor["transmissionPath"] = "1";

  return postMdsPayload(actconf, docpayload, "ota status");
}
