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

void DownloadFilesFromWeb(char *fversion)
{
  DownloadFile("css_black.css", fversion);
  DownloadFile("css_red.css", fversion);
  DownloadFile("css_white.css", fversion);
  DownloadFile("devinfo.html", fversion);
  DownloadFile("error.html", fversion);
  DownloadFile("favicon.ico", fversion);
  DownloadFile("firmware.html", fversion);
  DownloadFile("gauge.min.js", fversion);
  DownloadFile("header.html", fversion);
  DownloadFile("app.js", fversion);
  DownloadFile("lora.html", fversion);
  DownloadFile("index.html", fversion);
  DownloadFile("md5.js", fversion);
  DownloadFile("md5.min.js", fversion);
  DownloadFile("md5.min.js.map", fversion);
  DownloadFile("password.html", fversion);
  DownloadFile("restart.html", fversion);
  DownloadFile("sensorv.html", fversion);
  DownloadFile("settings.html", fversion);
  DownloadFile("settings.js", fversion);
}

void DownloadFile(const char *fileName, char *fversion)
{
  File targetFile;
  const char *host = actconf.firmwareUpdateUrl;
  const String myurl = "https://" + String(host) + "/files_for_esp_webserver/" + String(fversion) + "/" + String(fileName);
  WiFiClientSecure client;
  HTTPClient http;

  DebugPrint(3, "Connecting to website: ");
  DebugPrintln(3, String(host));
  DebugPrintln(3, myurl);

  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, myurl)) {
    DebugPrintln(1, "- failed to initialize HTTPS request");
    return;
  }

  Serial.print("[HTTP] GET...\n");
  const int httpCode = http.GET();

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
        return;
      }

      const int bytesWritten = http.writeToStream(&targetFile);
      targetFile.close();

      if (bytesWritten < 0) {
        DebugPrintln(1, "- failed to write downloaded file");
      }
    } else if (httpCode == HTTP_CODE_NOT_FOUND) {
      DebugPrintln(1, "- 404 page not found");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  DebugPrintln(3, "end.");
}

void sendToMDS(configData actconf)
{
  const String mdsUrl = String(actconf.MdsUrl);
  if (mdsUrl.length() == 0) {
    DebugPrintln(1, "MDS URL missing, skipping WiFi upload");
    return;
  }

  DynamicJsonDocument docpayload(2048);
  JsonObject board = docpayload.createNestedObject("board");
  JsonObject sensors = docpayload.createNestedObject("sensors");

  board["api_key"] = String(actconf.MdsApiKey);
  board["protocolVersion"] = "1";
  board["macAddress"] = WiFi.macAddress();
  board["deviceId"] = actconf.deviceID;
  board["firmwareVersion"] = String(actconf.fversion);
  board["wifiRssi"] = fieldstrength;
  board["wifiQuality"] = quality;

  struct tm tmstruct;
  if (getLocalTime(&tmstruct, 1000)) {
    char isoTimestamp[25];
    strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%S", &tmstruct);
    board["timestamp"] = isoTimestamp;
  }

  sensors["batteryVoltage"] = voltage;
  sensors["batteryCapacity"] = capacity;
  sensors["tank1Percent"] = tank1p;
  sensors["tank1Adc"] = tank1adc;
  sensors["tank2Percent"] = tank2p;
  sensors["tank2Adc"] = tank2adc;
  sensors["alarm1"] = alarm1;
  sensors["relay"] = actconf.relay;
  sensors["temp1wire"] = temp1wire;
  sensors["latitude"] = latitude;
  sensors["longitude"] = longitude;
  sensors["gpsStatus"] = gpsStatus;
  sensors["gpsSpeed"] = gpsspeed;
  sensors["course"] = course;

  if (String(actconf.envSensor) == "BME280") {
    sensors["airTemperature"] = temperature;
    sensors["airPressure"] = pressure;
    sensors["airHumidity"] = humidity;
    sensors["dewpoint"] = dewp;
    sensors["altitude"] = altitude;
  }

  if (String(actconf.envSensor) == "VEdirect-Read") {
    sensors["veDirectVoltage"] = vedirectVoltage;
    sensors["veDirectCurrent"] = vedirectCurrent;
    sensors["veDirectTemperature"] = vedirectTemp;
  }

  String requestBody;
  serializeJson(docpayload, requestBody);

  HTTPClient http;
  int httpResponseCode = -1;

  DebugPrintln(3, "Starting connection to server...");
  DebugPrintln(3, "MDS URL: " + mdsUrl);

  if (mdsUrl.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!http.begin(client, mdsUrl)) {
      DebugPrintln(1, "Failed to initialize HTTPS MDS request");
      return;
    }
    http.addHeader("Content-Type", "application/json");
    httpResponseCode = http.POST(requestBody);
  } else {
    WiFiClient client;
    if (!http.begin(client, mdsUrl)) {
      DebugPrintln(1, "Failed to initialize HTTP MDS request");
      return;
    }
    http.addHeader("Content-Type", "application/json");
    httpResponseCode = http.POST(requestBody);
  }

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
