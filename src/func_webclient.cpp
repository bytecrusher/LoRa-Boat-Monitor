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
#include <Configuration.h>
#include "func_myFunctions.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");

bool DownloadFilesFromWeb(char *fversion)
{
  const char *webFiles[] = {
    "css_black.css",
    "css_red.css",
    "css_white.css",
    "devinfo.html",
    "error.html",
    "favicon.ico",
    "firmware.html",
    "firmware_ota.html",
    "gauge.min.js",
    "header.html",
    "app.js",
    "lora.html",
    "index.html",
    "md5.js",
    "md5.min.js",
    "md5.min.js.map",
    "password.html",
    "restart.html",
    "sensorv.html",
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
  const char *host = actconf.firmwareUpdateUrl;
  const String myurl = "https://" + String(host) + "/files_for_esp_webserver/" + String(fversion) + "/" + String(fileName);
  WiFiClientSecure client;
  HTTPClient http;

  DebugPrint(3, "Connecting to website: ");
  DebugPrintln(3, String(host));
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

void sendToMDS(configData actconf)
{
  const String mdsUrl = String(actconf.MdsUrl);
  if (mdsUrl.length() == 0) {
    DebugPrintln(1, "MDS URL missing, skipping WiFi upload");
    return;
  }

  if (!mdsUrl.startsWith("https://")) {
    DebugPrintln(1, "Refusing insecure MDS URL. Please use https://");
    return;
  }

  JsonDocument docpayload;
  JsonObject board = docpayload["board"].to<JsonObject>();
  JsonArray sensors = docpayload["sensors"].to<JsonArray>();

  // MDS ingest expects board.apiKey.
  board["apiKey"] = String(actconf.MdsApiKey);
  board["protocolVersion"] = "1";
  board["macAddress"] = WiFi.macAddress();

  char mdsDate[11] = "01.01.1970";
  char mdsTime[9] = "00:00:00";
  struct tm tmstruct;
  if (getLocalTime(&tmstruct, 1000)) {
    strftime(mdsDate, sizeof(mdsDate), "%d.%m.%Y", &tmstruct);
    strftime(mdsTime, sizeof(mdsTime), "%H:%M:%S", &tmstruct);
  }

  const char* transmissionPath = "1";
  auto addSensorRecord = [&](int enabledMarker, const char* sensorType, const char* sensorName,
                             float value1, float value2, float value3, float value4) {
    if (enabledMarker <= 0 || sensorType == nullptr || sensorType[0] == '\0') {
      return;
    }
    JsonObject sensor = sensors.add<JsonObject>();
    sensor["sensorType"] = sensorType;
    if (sensorName != nullptr && sensorName[0] != '\0') {
      sensor["sensorName"] = sensorName;
    }
    sensor["value1"] = value1;
    sensor["value2"] = value2;
    sensor["value3"] = value3;
    sensor["value4"] = value4;
    sensor["date"] = mdsDate;
    sensor["time"] = mdsTime;
    sensor["transmissionPath"] = transmissionPath;
  };

  addSensorRecord(actconf.MdsSensorIdBattery, "ADC", "Battery", voltage, capacity, 0, 0);
  addSensorRecord(actconf.MdsSensorIdTanks, "ADC", "Tanks", tank1p, tank1adc, tank2p, tank2adc);
  addSensorRecord(actconf.MdsSensorIdStatus, "Digital", "Status", alarm1, actconf.relay, temp1wire, 0);
  addSensorRecord(actconf.MdsSensorIdGps, "GPS", "GPS", latitude, longitude, gpsspeed, course);

  if (String(actconf.envSensor) == "BME280") {
    addSensorRecord(actconf.MdsSensorIdEnv, "BME280", "Environment", temperature, humidity, pressure, altitude);
    addSensorRecord(actconf.MdsSensorIdDewpoint, "BME280", "Dewpoint", dewp, 0, 0, 0);
  }

  if (String(actconf.envSensor) == "VEdirect-Read") {
    addSensorRecord(actconf.MdsSensorIdVedirect, "DS2438", "VEdirect", vedirectVoltage, vedirectCurrent, vedirectTemp, 0);
  }

  String requestBody;
  serializeJson(docpayload, requestBody);

  HTTPClient http;
  int httpResponseCode = -1;

  DebugPrintln(3, "Starting connection to server...");
  DebugPrintln(3, "MDS URL: " + mdsUrl);

  WiFiClientSecure client;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  if (!http.begin(client, mdsUrl)) {
    DebugPrintln(1, "Failed to initialize HTTPS MDS request");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  httpResponseCode = http.POST(requestBody);

  DebugPrint(3, "HTTP POST payload: ");
  DebugPrintln(3, requestBody);
  DebugPrint(3, "HTTP response: ");
  DebugPrintln(3, httpResponseCode);

  if (httpResponseCode > 0) {
    DebugPrintln(3, http.getString());
  } else {
    DebugPrintln(1, "WiFi upload failed: " + http.errorToString(httpResponseCode));
  }

  http.end();
}
