#include "func_webclient.h"
#include <WiFi.h>       // WiFi lib with TCP server and client
#include <WiFiClient.h> // WiFi lib for clients
#include <WiFiClientSecure.h>

#include <ESP32_FTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <mbedtls/error.h>
#include <Configuration.h>
#include "func_myFunctions.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");

namespace {
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

  String requestBody;
  serializeJson(docpayload, requestBody);
  WiFiClientSecure client;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  client.setTimeout(15000);

  DebugPrintln(3, String("Starting MDS request (") + contextLabel + ")");
  DebugPrintln(3, "MDS URL: " + mdsUrl);

  String urlWithoutScheme = mdsUrl;
  urlWithoutScheme.remove(0, 8);
  const int pathStart = urlWithoutScheme.indexOf('/');
  const String host = pathStart >= 0 ? urlWithoutScheme.substring(0, pathStart) : urlWithoutScheme;
  const String path = pathStart >= 0 ? urlWithoutScheme.substring(pathStart) : "/";

  if (!client.connect(host.c_str(), 443)) {
    DebugPrintln(1, "Failed to connect to MDS host: " + host);
    return false;
  }
  
  client.print(String("POST ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("User-Agent: LoRaBoatMonitor/ESP32\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print(String("Content-Length: ") + requestBody.length() + "\r\n\r\n");
  client.print(requestBody);

  DebugPrint(3, "HTTP POST payload: ");
  DebugPrintln(3, requestBody);

  const String statusLine = client.readStringUntil('\n');
  String trimmedStatusLine = statusLine;
  trimmedStatusLine.trim();
  DebugPrintln(3, "HTTP status line: " + trimmedStatusLine);

  int httpResponseCode = -1;
  const int firstSpace = trimmedStatusLine.indexOf(' ');
  if (firstSpace >= 0) {
    const int secondSpace = trimmedStatusLine.indexOf(' ', firstSpace + 1);
    const String codeToken = secondSpace >= 0
      ? trimmedStatusLine.substring(firstSpace + 1, secondSpace)
      : trimmedStatusLine.substring(firstSpace + 1);
    httpResponseCode = codeToken.toInt();
  }

  while (client.connected()) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r" || headerLine.length() == 0) {
      break;
    }
  }

  const String responseBody = client.readString();
  DebugPrint(3, "HTTP response: ");
  DebugPrintln(3, httpResponseCode);
  if (responseBody.length() > 0) {
    DebugPrintln(3, responseBody);
  }
  client.stop();

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
  } else {
    DebugPrintln(1, "WiFi upload failed before a valid HTTP response was received");
  }
  return success;
}
}

bool DownloadFilesFromWeb(char *fversion)
{
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
    "firmware_ota.html",
    "firmware_ota.css",
    "firmware_ota.js",
    "gauge.min.js",
    "header.html",
    "app.js",
    "lora.html",
    "index.html",
    "index.js",
    "md5.js",
    "md5.min.js",
    "md5.min.js.map",
    "lora.js",
    "password.html",
    "restart.html",
    "sensorv.html",
    "sensorv.js",
    "settings-page.css",
    "settings.html",
    "settings.js"
  };

  bool allFilesUpdated = true;
  for (size_t i = 0; i < (sizeof(webFiles) / sizeof(webFiles[0])); ++i) {
    allFilesUpdated = DownloadFile(webFiles[i], fversion) && allFilesUpdated;
  }

  if (allFilesUpdated) {
    saveWebFilesVersion(fversion);
  }

  return allFilesUpdated;
}

bool DownloadFile(const char *fileName, char *fversion)
{
  File targetFile;
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
      const String targetPath = "/" + String(fileName);
      if (LittleFS.exists(targetPath)) {
        LittleFS.remove(targetPath);
      }

      targetFile = LittleFS.open(targetPath.c_str(), FILE_WRITE);
      if (!targetFile) {
        DebugPrintln(1, "- failed to open file for writing");
        http.end();
        return false;
      }

      const int bytesWritten = http.writeToStream(&targetFile);
      targetFile.close();

      if (bytesWritten < 0) {
        DebugPrintln(1, "- failed to write downloaded file");
      } else {
        downloadSuccess = true;
      }
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

bool sendMdsDeviceEvent(configData actconf, const char *sensorName, float value1, float value2, float value3, float value4)
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

  addMdsSensorRecord(sensors, 1, "Digital", sensorName, mdsDate, mdsTime, "1", value1, value2, value3, value4);
  return postMdsPayload(actconf, docpayload, sensorName == nullptr ? "device event" : sensorName);
}
