#include "func_webclient.h"
#include <WiFi.h>       // WiFi lib with TCP server and client
#include <WiFiClient.h> // WiFi lib for clients
#include <WiFiClientSecure.h>

#include "FS.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <time.h>
#include <Configuration.h>
#include "func_myFunctions.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");
extern size_t webFilesDownloadCompleted;
extern size_t webFilesDownloadTotal;
extern String webFilesDownloadCurrentName;
extern time_t lastStandbyEventEpoch;
extern time_t lastWakeupEventEpoch;
extern char lastStandbyEventCause[24];
extern char lastWakeupEventCause[24];
extern bool pendingMdsDeviceEventStored;
extern time_t pendingMdsStandbyEventEpoch;
extern time_t pendingMdsWakeupEventEpoch;
extern char pendingMdsStandbyEventCause[24];
extern char pendingMdsWakeupEventCause[24];

namespace {
const size_t MAX_WEB_FILE_DOWNLOAD_SIZE = 262144;

String normalizeSha256(const String &sha256) {
  String normalized = sha256;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized.length() != 64) {
    return "";
  }

  for (size_t i = 0; i < normalized.length(); i++) {
    const char c = normalized[i];
    if (!isxdigit(c)) {
      return "";
    }
  }

  return normalized;
}

String sha256ToHex(const uint8_t digest[32]) {
  static const char hexChars[] = "0123456789abcdef";
  String hex;
  hex.reserve(64);
  for (size_t i = 0; i < 32; i++) {
    hex += hexChars[(digest[i] >> 4) & 0x0F];
    hex += hexChars[digest[i] & 0x0F];
  }
  return hex;
}

String normalizeFirmwareUpdateBaseUrl(const char *configuredValue)
{
  String normalized = String(configuredValue == nullptr ? "" : configuredValue);
  normalized.trim();

  if (normalized.startsWith("https://")) {
    normalized.remove(0, 8);
  } else if (normalized.startsWith("http://")) {
    normalized.remove(0, 7);
  }

  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }

  const String suffix = "/files_for_esp_webserver";
  if (normalized.endsWith(suffix)) {
    normalized.remove(normalized.length() - suffix.length());
  }

  return "https://" + normalized + suffix;
}

bool fetchTextFromUpdateServer(const String &url, String &payload) {
  WiFiClientSecure client;
  HTTPClient http;

  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);

  if (!http.begin(client, url)) {
    DebugPrintln(1, "- failed to initialize HTTPS manifest request");
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    DebugPrint(1, "Manifest request failed: ");
    DebugPrintln(1, httpCode);
    http.end();
    return false;
  }

  payload = http.getString();
  payload.trim();
  http.end();
  return payload.length() > 0;
}

bool fetchFirmwareManifest(JsonDocument &manifest) {
  const String baseUrl = normalizeFirmwareUpdateBaseUrl(actconf.firmwareUpdateUrl);
  const String manifestUrl = baseUrl + "/firmware-manifest.json";
  String manifestPayload;
  if (!fetchTextFromUpdateServer(manifestUrl, manifestPayload)) {
    return false;
  }

  DeserializationError error = deserializeJson(manifest, manifestPayload);
  if (error) {
    DebugPrint(1, "Firmware manifest JSON error: ");
    DebugPrintln(1, error.c_str());
    return false;
  }

  return true;
}

String getExpectedWebFileSha256(JsonDocument &manifest, const char *fversion, const char *fileName) {
  const char *channels[] = {"stable", "beta"};
  for (const char *channel : channels) {
    JsonObject entry = manifest[channel].as<JsonObject>();
    if (entry.isNull()) {
      continue;
    }
    const char *version = entry["version"] | "";
    if (String(version) != String(fversion)) {
      continue;
    }
    const char *sha256 = entry["webFileHashes"][fileName] | "";
    return normalizeSha256(String(sha256));
  }

  return "";
}

String normalizeSecureUrl(const String &configuredValue)
{
  String normalized = configuredValue;
  normalized.trim();

  if (normalized.startsWith("http://")) {
    normalized.remove(0, 7);
    normalized = "https://" + normalized;
  }

  return normalized;
}

bool hasValidSystemTime()
{
  const time_t now = time(nullptr);
  return now >= 1704067200; // 2024-01-01 00:00:00 UTC
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

bool postMdsPayload(configData actconf, JsonDocument &docpayload, const char *contextLabel)
{
  const String mdsUrl = normalizeSecureUrl(String(actconf.MdsUrl));
  if (mdsUrl.length() == 0) {
    DebugPrintln(1, "MDS URL missing, skipping WiFi upload");
    return false;
  }

  if (!mdsUrl.startsWith("https://")) {
    DebugPrintln(1, "Refusing insecure MDS URL. Please use https://");
    return false;
  }

  if (!hasValidSystemTime()) {
    DebugPrintln(2, "Skipping MDS request because system time is not synchronized yet");
    return false;
  }

  String requestBody;
  serializeJson(docpayload, requestBody);
  JsonDocument debugPayload;
  deserializeJson(debugPayload, requestBody);
  if (debugPayload["board"]["apiKey"].is<String>()) {
    debugPayload["board"]["apiKey"] = "***hidden***";
  }
  String debugRequestBody;
  serializeJson(debugPayload, debugRequestBody);

  auto handleMdsResponse = [&](int httpResponseCode, const String &responseBody) -> bool {
    DebugPrint(3, "HTTP response: ");
    DebugPrintln(3, httpResponseCode);
    if (responseBody.length() > 0) {
      DebugPrintln(3, responseBody);
    }

    bool success = false;
    if (httpResponseCode > 0) {
      JsonDocument responseJson;
      if (deserializeJson(responseJson, responseBody) == DeserializationError::Ok) {
        if (responseJson["insertedSensorRows"].is<int>() || responseJson["skippedSensorRows"].is<int>()) {
          DebugPrintln(3, String("MDS inserted rows: ") + int(responseJson["insertedSensorRows"] | 0));
          DebugPrintln(3, String("MDS skipped rows: ") + int(responseJson["skippedSensorRows"] | 0));
          DebugPrintln(3, String("MDS auto resolved rows: ") + int(responseJson["autoResolvedSensorRows"] | 0));
          DebugPrintln(3, String("MDS auto created sensor configs: ") + int(responseJson["autoCreatedSensorConfigs"] | 0));
          success = (httpResponseCode == HTTP_CODE_OK) && ((int(responseJson["insertedSensorRows"] | 0)) > 0);
        } else {
          success = (httpResponseCode == HTTP_CODE_OK);
        }
      } else {
        success = (httpResponseCode == HTTP_CODE_OK);
      }
    }
    return success;
  };

  WiFiClientSecure client;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  client.setHandshakeTimeout(15);
  client.setTimeout(15);

  DebugPrintln(3, String("Starting MDS request (") + contextLabel + ")");
  DebugPrintln(3, "MDS URL: " + mdsUrl);

  String urlWithoutScheme = mdsUrl;
  urlWithoutScheme.remove(0, 8);
  const int pathStart = urlWithoutScheme.indexOf('/');
  const String host = pathStart >= 0 ? urlWithoutScheme.substring(0, pathStart) : urlWithoutScheme;
  const String path = pathStart >= 0 ? urlWithoutScheme.substring(pathStart) : "/";

  IPAddress resolvedIp;
  if (!WiFi.hostByName(host.c_str(), resolvedIp)) {
    DebugPrintln(1, "DNS lookup failed for MDS host: " + host);
    return false;
  }
  DebugPrintln(3, "MDS host resolved to: " + resolvedIp.toString());

  DebugPrint(3, "HTTP POST payload: ");
  DebugPrintln(3, debugRequestBody.length() > 0 ? debugRequestBody : String("***hidden***"));

  if (!client.connect(resolvedIp, 443, host.c_str(), reinterpret_cast<const char*>(cert_cacert_pem_start), nullptr, nullptr)) {
    char errorBuffer[128] = {0};
    client.lastError(errorBuffer, sizeof(errorBuffer));
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

bool DownloadFilesFromWeb()
{
  const char *fversion = actconf.fversion;
  DebugPrintln(3, "Downloading web files for installed firmware version: " + String(fversion));

  const char *webFiles[] = {
    "css_black.css",
    "css_red.css",
    "css_white.css",
    "common.css",
    "common.js",
    "devinfo.html",
    "error.html",
    "favicon.ico",
    "firmware.html",
    "firmware-page.js",
    "firmware_ota.html",
    "firmware_ota.css",
    "firmware_ota.js",
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

  JsonDocument manifest;
  if (!fetchFirmwareManifest(manifest)) {
    DebugPrintln(1, "Unable to fetch firmware manifest for web file hash validation");
    return false;
  }

  webFilesDownloadCompleted = 0;
  webFilesDownloadTotal = sizeof(webFiles) / sizeof(webFiles[0]);
  webFilesDownloadCurrentName = "";

  bool allFilesUpdated = true;
  for (size_t i = 0; i < webFilesDownloadTotal; ++i) {
    webFilesDownloadCurrentName = String(webFiles[i]);
    const String expectedSha256 = getExpectedWebFileSha256(manifest, fversion, webFiles[i]);
    if (expectedSha256.length() == 0) {
      DebugPrint(1, "Missing manifest hash for web file: ");
      DebugPrintln(1, webFiles[i]);
      allFilesUpdated = false;
    } else {
      allFilesUpdated = DownloadFile(webFiles[i], fversion, expectedSha256) && allFilesUpdated;
    }
    webFilesDownloadCompleted = i + 1;
  }

  webFilesDownloadCurrentName = "";

  if (allFilesUpdated) {
    saveWebFilesVersion(fversion);
  }

  return allFilesUpdated;
}

bool DownloadFile(const char *fileName, const char *fversion, const String &expectedSha256)
{
  const String normalizedExpectedSha256 = normalizeSha256(expectedSha256);
  if (normalizedExpectedSha256.length() == 0) {
    DebugPrint(1, "Refusing web file download without valid SHA256: ");
    DebugPrintln(1, fileName);
    return false;
  }

  const String baseUrl = normalizeFirmwareUpdateBaseUrl(actconf.firmwareUpdateUrl);
  const String myurl = baseUrl + "/" + String(fversion) + "/" + String(fileName);
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

      const String targetPath = "/" + String(fileName);
      const String tempPath = targetPath + ".tmp";
      if (LittleFS.exists(tempPath)) {
        LittleFS.remove(tempPath);
      }

      File targetFile = LittleFS.open(tempPath.c_str(), FILE_WRITE);
      if (!targetFile) {
        DebugPrintln(1, "- failed to open file for writing");
        http.end();
        return false;
      }

      WiFiClient *stream = http.getStreamPtr();
      uint8_t buffer[1024];
      size_t written = 0;
      mbedtls_sha256_context shaContext;
      mbedtls_sha256_init(&shaContext);
      mbedtls_sha256_starts_ret(&shaContext, 0);

      while (http.connected() && written < static_cast<size_t>(contentLength)) {
        const size_t availableBytes = stream->available();
        if (availableBytes == 0) {
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
          LittleFS.remove(tempPath);
          http.end();
          return false;
        }

        mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
        written += bytesRead;
      }

      targetFile.close();

      uint8_t digest[32];
      mbedtls_sha256_finish_ret(&shaContext, digest);
      mbedtls_sha256_free(&shaContext);

      if (written != static_cast<size_t>(contentLength)) {
        DebugPrintln(1, "- incomplete web file download");
        LittleFS.remove(tempPath);
        http.end();
        return false;
      }

      const String actualSha256 = sha256ToHex(digest);
      if (actualSha256 != normalizedExpectedSha256) {
        DebugPrint(1, "Web file checksum mismatch: ");
        DebugPrintln(1, fileName);
        LittleFS.remove(tempPath);
        http.end();
        return false;
      }

      if (LittleFS.exists(targetPath)) {
        LittleFS.remove(targetPath);
      }
      if (!LittleFS.rename(tempPath, targetPath)) {
        DebugPrintln(1, "- failed to install verified web file");
        LittleFS.remove(tempPath);
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

bool sendToMDS(configData actconf)
{
  if (WiFi.status() != WL_CONNECTED) {
    DebugPrintln(2, "Skipping MDS sensor upload because WiFi is not connected");
    return false;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  board["apiKey"] = String(actconf.MdsApiKey);
  board["protocolVersion"] = "1";
  board["macAddress"] = WiFi.macAddress();

  char mdsDate[11];
  char mdsTime[9];
  buildMdsTimestamp(mdsDate, mdsTime);

  const char* transmissionPath = "1";
  addMdsSensorRecord(sensors, actconf.MdsSensorIdBattery, "ADC", "Battery", mdsDate, mdsTime, transmissionPath, voltage, capacity, 0, 0);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdTanks, "ADC", "Tanks", mdsDate, mdsTime, transmissionPath, tank1p, tank1adc, tank2p, tank2adc);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdStatus, "Digital", "Status", mdsDate, mdsTime, transmissionPath, alarm1, actconf.relay, temp1wire, 0);
  addMdsSensorRecord(sensors, actconf.MdsSensorIdGps, "GPS", "GPS", mdsDate, mdsTime, transmissionPath, latitude, longitude, gpsspeed, course);

  if (String(actconf.envSensor) == "BME280") {
    addMdsSensorRecord(sensors, actconf.MdsSensorIdEnv, "BME280", "Environment", mdsDate, mdsTime, transmissionPath, temperature, humidity, pressure, altitude);
    addMdsSensorRecord(sensors, actconf.MdsSensorIdDewpoint, "BME280", "Dewpoint", mdsDate, mdsTime, transmissionPath, dewp, 0, 0, 0);
  }

  if (String(actconf.envSensor) == "VEdirect-Read") {
    addMdsSensorRecord(sensors, actconf.MdsSensorIdVedirect, "DS2438", "VEdirect", mdsDate, mdsTime, transmissionPath, vedirectVoltage, vedirectCurrent, vedirectTemp, 0);
  }

  if (sensors.size() == 0) {
    DebugPrintln(2, "Skipping MDS sensor upload because no MDS uploads are enabled");
    return false;
  }

  return postMdsPayload(actconf, docpayload, "sensor upload");
}

bool sendMdsDeviceEvent(configData actconf, const char *sensorName)
{
  if (WiFi.status() != WL_CONNECTED) {
    DebugPrintln(2, String("Skipping MDS device event because WiFi is not connected: ") + String(sensorName == nullptr ? "" : sensorName));
    return false;
  }

  if (actconf.MdsSensorIdStatus <= 0) {
    DebugPrintln(2, "Skipping MDS device event because status upload is disabled");
    return false;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  board["apiKey"] = String(actconf.MdsApiKey);
  board["protocolVersion"] = "1";
  board["macAddress"] = WiFi.macAddress();

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
