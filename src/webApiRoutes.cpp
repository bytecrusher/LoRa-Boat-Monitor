#include "func_webServerHandler.h"
#include "webServerRoutes.h"
#include "firmwareBootHealth.h"
#include "updateRuntimeState.h"
#include <WiFi.h>

extern int bootCount;

void registerApiRoutes() {
  httpServer.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) {
      return;
    }
    JsonDocument json;
    json["status"] = "ok";
    json["firmwareVersion"] = String(actconf.fversion);
    json["uptimeMs"] = millis();
    json["freeHeap"] = ESP.getFreeHeap();
    json["minFreeHeap"] = ESP.getMinFreeHeap();
    json["resetReason"] = static_cast<int>(esp_reset_reason());
    json["bootCount"] = bootCount;
    json["bootValidationPending"] = isFirmwareBootValidationPending();
    json["bootValidationAttempts"] = getFirmwareBootAttemptCount();
    json["bootHealth"] = getFirmwareBootHealthStatus();
    json["recoverySafeMode"] = isRecoverySafeMode();
    json["webInstallCheckpoint"] = getWebBundleInstallCheckpoint();
    json["webInstallFreeHeap"] = getWebBundleInstallFreeHeap();
    json["webInstallStackWords"] = getWebBundleInstallStackWords();
    json["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    json["ip"] = WiFi.localIP().toString();
    json["wifiRssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    json["maintenanceOperation"] = maintenanceOperationName(getMaintenanceOperation());
    const OtaProgressSnapshot ota = getOtaProgressSnapshot();
    json["otaActive"] = ota.active;
    json["otaPhase"] = ota.phase;
    json["otaCurrent"] = ota.current;
    json["otaTotal"] = ota.total;
    const WebFilesUpdateSnapshot webFiles = getWebFilesUpdateSnapshot();
    json["webFilesUpdateActive"] = webFiles.active;
    json["webFilesUpdateMessage"] = webFiles.message;
    json["configStorage"] = "CFG2 A/B";
    const String mainPowerLevel = digitalRead(mainPowerInputPin) == LOW ? "LOW" : "HIGH";
    const String mainPowerMode = mainPowerOn ? "Always on" : "Sleep/wakeup";
    json["mainPowerInputPin"] = mainPowerInputPin;
    json["mainPowerInputLevel"] = mainPowerLevel;
    json["mainPowerMode"] = mainPowerMode;
    // Compatibility aliases for web files from earlier firmware releases.
    json["standbyInputPin"] = mainPowerInputPin;
    json["standbyInputLevel"] = mainPowerLevel;
    json["standbyInputState"] = mainPowerMode;

    String responseBody;
    serializeJson(json, responseBody);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", responseBody);
    response->addHeader("Cache-Control", "no-store, max-age=0");
    request->send(response);
  });

  httpServer.on("/staticdata.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    JsonDocument json_Device;
    json_Device["Device"]["Type"] = String(actconf.devname);
    json_Device["Device"]["CopyRights"] = String(actconf.crights);
    json_Device["Device"]["FirmwareVersion"] = String(actconf.fversion);
    json_Device["Device"]["FirmwareChannel"] = getFirmwareReleaseChannel();
    json_Device["Device"]["FirmwareChannelLabel"] = getFirmwareReleaseLabel();
    json_Device["Device"]["UpdateChannel"] = getConfiguredUpdateChannel();
    json_Device["Device"]["UpdateChannelLabel"] = getConfiguredUpdateLabel();
    json_Device["Device"]["License"] = String(actconf.license);

    json_Device["Device"]["ESP32"]["SDKVersion"] = String(ESP.getSdkVersion());
    json_Device["Device"]["ESP32"]["ChipID"] = String(chipId);
    json_Device["Device"]["ESP32"]["CPUSpeed"]["Value"] = String(ESP.getCpuFreqMHz());
    json_Device["Device"]["ESP32"]["CPUSpeed"]["Unit"] = "MHz";

    json_Device["Device"]["NetworkParameter"]["WLANClientSSID1"] = String(actconf.cssid1);
    json_Device["Device"]["NetworkParameter"]["WLANClientSSID2"] = String(actconf.cssid2);
    json_Device["Device"]["NetworkParameter"]["WLANClientSSID3"] = String(actconf.cssid3);
    json_Device["Device"]["NetworkParameter"]["WLANClientIP"] = WiFi.localIP().toString();

    json_Device["Device"]["NetworkParameter"]["WLANServerSSID"] = String(actconf.sssid);
    json_Device["Device"]["NetworkParameter"]["WLANServerIP"] = WiFi.softAPIP().toString();
    json_Device["Device"]["NetworkParameter"]["ServerMode"] = String(actconf.serverMode);
    json_Device["Device"]["NetworkParameter"]["ServerHostName"] = String(actconf.hostname);

    json_Device["Device"]["NetworkParameter"]["MdsUrl"] = String(actconf.MdsUrl);
    json_Device["Device"]["NetworkParameter"]["MdsApiKey"] = String(strlen(actconf.MdsApiKey) > 0 ? "***hidden***" : "");

    // Unused ?
    //json_Device["Device"]["DeviceSettings"]["SerialDebugMode"] = String(actconf.debug);
    //json_Device["Device"]["DeviceSettings"]["SerialSpeed"] = String(actconf.serspeed);
    json_Device["Device"]["DeviceSettings"]["WebSerialDebug"] = String(actconf.WebSerialDebug);
    //json_Device["Device"]["DeviceSettings"]["DeviceID"] = String(actconf.deviceID);
    //json_Device["Device"]["DeviceSettings"]["SendData"] = String(actconf.senddata);
    //json_Device["Device"]["DeviceSettings"]["VoltageOffset"] = String(actconf.voffset);
    //json_Device["Device"]["DeviceSettings"]["VoltageSlopeA1"] = String(actconf.a1vslope);
    //json_Device["Device"]["DeviceSettings"]["VoltageSlopeA2"] = String(actconf.a2vslope);
    //json_Device["Device"]["DeviceSettings"]["VoltageAverage"] = String(actconf.vaverage);
    //json_Device["Device"]["DeviceSettings"]["Tank1Offset"] = String(actconf.t1offset);
    //json_Device["Device"]["DeviceSettings"]["Tank1SlopeA1"] = String(actconf.a1t1slope);
    //json_Device["Device"]["DeviceSettings"]["Tank1SlopeA2"] = String(actconf.a2t1slope);
    //json_Device["Device"]["DeviceSettings"]["Tank1Average"] = String(actconf.t1average);
    //json_Device["Device"]["DeviceSettings"]["Tank2Offset"] = String(actconf.t2offset);
    //json_Device["Device"]["DeviceSettings"]["Tank2SlopeA1"] = String(actconf.a1t2slope);
    //json_Device["Device"]["DeviceSettings"]["Tank2SlopeA2"] = String(actconf.a2t2slope);
    //json_Device["Device"]["DeviceSettings"]["Tank2Average"] = String(actconf.t2average);
    json_Device["Device"]["DeviceSettings"]["TempSensorType"] = String(actconf.tempSensorType);
    json_Device["Device"]["DeviceSettings"]["TempUnit"] = String(actconf.tempUnit);
    json_Device["Device"]["DeviceSettings"]["envSensor"] = String(actconf.envSensor);
    json_Device["Device"]["DeviceSettings"]["standbyMode"] = String(actconf.standbyMode);
    json_Device["Device"]["DeviceSettings"]["transmitPriority"] = String(actconf.transmitPriority);
    json_Device["Device"]["DeviceSettings"]["standbyAutoUpdate"] = String(actconf.standbyAutoUpdate);
    json_Device["Device"]["DeviceSettings"]["standbyAutoUpdateIntervalHours"] = actconf.standbyAutoUpdateIntervalHours;

    String stringjsondata = "";
    serializeJson(json_Device, stringjsondata);

    request->send(200, "application/json", stringjsondata);
  });

  httpServer.on("/lora/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1 && !request->authenticate(actconf.username, actconf.password)) {
      return request->requestAuthentication();
    }

    JsonDocument response;
    buildManualLoraStatus(response);
    String body;
    serializeJson(response, body);
    request->send(200, "application/json", body);
  });

  httpServer.on("/lora/send", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1 && !request->authenticate(actconf.username, actconf.password)) {
      return request->requestAuthentication();
    }
    if (!requireCsrfToken(request)) {
      return;
    }

    noteWebApiActivity();
    String message;
    const bool queued = queueManualLoraSend(message);
    JsonDocument response;
    response["status"] = queued ? "queued" : "error";
    response["message"] = message;
    String body;
    serializeJson(response, body);
    request->send(queued ? 202 : 409, "application/json", body);
  });

  httpServer.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    //unsigned long previousMillis = 0;
    //unsigned long elapsedMillis = 0;
    //previousMillis = millis();

    JsonDocument json_Device;
    json_Device["Device"]["ESP32"]["FreeHeapSize"]["Value"] = String(ESP.getFreeHeap());
    json_Device["Device"]["ESP32"]["FreeHeapSize"]["Unit"] = "Byte";  // TODO: bring into staticdata.json

    json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Value"] = String(fieldstrength);
    json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Unit"] = "dBm";  // TODO: bring into staticdata.json

    json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Value"] = String(int(quality));
    json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Unit"] = "%";  // TODO: bring into staticdata.json

    String mydevaddr = String(actconf.devaddr, HEX);
    mydevaddr.toUpperCase();
    json_Device["Device"]["LoRaSettings"]["DeviceAddress"] = mydevaddr;  // TODO: bring into staticdata.json
    json_Device["Device"]["LoRaSettings"]["Frequency"] = String(actconf.lorafrequency);
    json_Device["Device"]["LoRaSettings"]["Channel"] = String(actconf.lchannel);
    json_Device["Device"]["LoRaSettings"]["ActualChannel"] = String(getLMICtxChnl());
    json_Device["Device"]["LoRaSettings"]["SpreadingFactor"] = String(actconf.spreadf);
    json_Device["Device"]["LoRaSettings"]["ActualSF"] = String(sf);
    json_Device["Device"]["LoRaSettings"]["DynamicSF"] = String(actconf.dynsf);
    json_Device["Device"]["LoRaSettings"]["TXInterval"] = String(actconf.tinterval);
    json_Device["Device"]["LoRaSettings"]["TimeSlot"] = String(slot);
    json_Device["Device"]["LoRaSettings"]["TXCounter"] = String(getLMICseqnoUp());
    json_Device["Device"]["LoRaSettings"]["Relay"] = String(actconf.relay);

    json_Device["Device"]["DisplaySettings"]["Skin"] = String(actconf.skin);
    json_Device["Device"]["DisplaySettings"]["InstrumentSize"] = String(actconf.instrumentSize);
    json_Device["Device"]["DisplaySettings"]["OledDisplayMode"] = String(actconf.OledDisplayMode);

    json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Value"] = String(temperature, 1);
    json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["AirPressure"]["Value"] = String(pressure, 0);
    json_Device["Device"]["MeasuringValues"]["AirPressure"]["Unit"] = "mbar";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Value"] = String(humidity, 0);
    json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Value"] = String(dewp, 1);
    json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Value"] = String(temp1wire, 1);
    json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Value"] = String(voltage, 3);
    json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["BatteryAdc"]["Value"] = String(voltageadc);

    json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Value"] = String(capacity, 0);
    json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Value"] = String(tank1, 3);
    json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Value"] = String(tank2, 3);
    json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1"]["Value"] = String(tank1p, 3);
    json_Device["Device"]["MeasuringValues"]["Tank1"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1adc"]["Value"] = String(tank1adc);

    json_Device["Device"]["MeasuringValues"]["Tank2"]["Value"] = String(tank2p, 3);
    json_Device["Device"]["MeasuringValues"]["Tank2"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank2adc"]["Value"] = String(tank2adc);

    json_Device["Device"]["MeasuringValues"]["MainPowerOn"]["Value"] = String(mainPowerOn);
    json_Device["Device"]["MeasuringValues"]["MainPowerOn"]["Unit"] = "bool";

    const int mainPowerRawLevel = digitalRead(mainPowerInputPin);
    const String mainPowerLevel = mainPowerRawLevel == LOW ? "LOW" : "HIGH";
    const String mainPowerMode = mainPowerOn ? "Always on" : "Sleep/wakeup";
    json_Device["Device"]["MeasuringValues"]["MainPowerInputPin"]["Value"] = String(mainPowerInputPin);
    json_Device["Device"]["MeasuringValues"]["MainPowerInputPin"]["Unit"] = "GPIO";
    json_Device["Device"]["MeasuringValues"]["MainPowerInputLevel"]["Value"] = mainPowerLevel;
    json_Device["Device"]["MeasuringValues"]["MainPowerInputLevel"]["Unit"] = "12 V = LOW";
    json_Device["Device"]["MeasuringValues"]["MainPowerMode"]["Value"] = mainPowerMode;
    json_Device["Device"]["MeasuringValues"]["MainPowerMode"]["Unit"] = mainPowerOn ? "12 V present" : "no 12 V";

    // Compatibility aliases keep older web packages operational during an update.
    json_Device["Device"]["MeasuringValues"]["Alarm"]["Value"] = String(mainPowerOn);
    json_Device["Device"]["MeasuringValues"]["Alarm"]["Unit"] = "bin";
    json_Device["Device"]["MeasuringValues"]["StandbyInputPin"] = json_Device["Device"]["MeasuringValues"]["MainPowerInputPin"];
    json_Device["Device"]["MeasuringValues"]["StandbyInputLevel"] = json_Device["Device"]["MeasuringValues"]["MainPowerInputLevel"];
    json_Device["Device"]["MeasuringValues"]["StandbyInputState"] = json_Device["Device"]["MeasuringValues"]["MainPowerMode"];

    json_Device["Device"]["MeasuringValues"]["Relay"]["Value"] = String(actconf.relay);
    json_Device["Device"]["MeasuringValues"]["Relay"]["Unit"] = "bin";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Value"] = String(int(relaytimer * 5));
    json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Unit"] = "x5min";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Value"] = String(actconf.envSensor);
    json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["standbyMode"]["Value"] = String(actconf.standbyMode);
    json_Device["Device"]["MeasuringValues"]["standbyMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["loraOperationMode"]["Value"] = String(actconf.loraOperationMode);
    json_Device["Device"]["MeasuringValues"]["loraOperationMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["WifiStandbyMode"]["Value"] = String(actconf.WifiStandbyMode);
    json_Device["Device"]["MeasuringValues"]["WifiStandbyMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["SendDataViaWifi"]["Value"] = String(actconf.SendDataViaWifi);
    json_Device["Device"]["MeasuringValues"]["SendDataViaWifi"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["TransmitPriority"]["Value"] = String(actconf.transmitPriority);
    json_Device["Device"]["MeasuringValues"]["TransmitPriority"]["Unit"] = "";

    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Value"] = String(vedirectVoltage, 3);
    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Value"] = String(vedirectCurrent, 3);
    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Unit"] = "A";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Value"] = String(vedirectTemp, 1);
    json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Unit"] = "°C";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Latitude"]["Value"] = String(latitude, 6);
    json_Device["Device"]["MeasuringValues"]["Latitude"]["Unit"] = String(latitudeNS);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Longitude"]["Value"] = String(longitude, 6);
    json_Device["Device"]["MeasuringValues"]["Longitude"]["Unit"] = String(longitudeEW);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Altitude"]["Value"] = String(altitude, 0);
    json_Device["Device"]["MeasuringValues"]["Altitude"]["Unit"] = "m";  // TODO: bring into staticdata.json

    String zhour = firstzero(hour);
    String zminute = firstzero(minute);
    String zsecond = firstzero(second);
    String timestr = String(zhour) + ":" + String(zminute) + ":" + String(zsecond);
    json_Device["Device"]["MeasuringValues"]["Time"]["Value"] = String(timestr);
    json_Device["Device"]["MeasuringValues"]["Time"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    String zday = firstzero(day);
    String zmonth = firstzero(month);
    String zyear = firstzero(year);
    String datestr = String(zday) + "." + String(zmonth) + "." + String(zyear);
    json_Device["Device"]["MeasuringValues"]["Date"]["Value"] = String(datestr);
    json_Device["Device"]["MeasuringValues"]["Date"]["Unit"] = "GMT";  // TODO: bring into staticdata.json

    // Sunrise and sunset are not calculated by the firmware. Do not expose
    // the current clock time under misleading field names.
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Value"] = "-";
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Unit"] = "";
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Value"] = "-";
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Unit"] = "";

    json_Device["Device"]["MeasuringValues"]["Speed"]["Value"] = String(gpsspeed);
    json_Device["Device"]["MeasuringValues"]["Speed"]["Unit"] = "kn";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Course"]["Value"] = String(course);
    json_Device["Device"]["MeasuringValues"]["Course"]["Unit"] = "°";  // TODO: bring into staticdata.json

    // for debugging purpose?
    //json_Device["Device"]["NMEAValues"]["String1"] = sendXDR1(0);
    //json_Device["Device"]["NMEAValues"]["String2"] = sendXDR2(0);
    //json_Device["Device"]["NMEAValues"]["String3"] = sendXDR3(0);
    //json_Device["Device"]["NMEAValues"]["String4"] = "°";
    //json_Device["Device"]["NMEAValues"]["String5"] = "°";

    String stringjsondata = "";
    serializeJson(json_Device, stringjsondata);

    request->send(200, "application/json", stringjsondata);

    //elapsedMillis = millis() - previousMillis;
    //DebugPrint(3, "Wert Stoppuhr: " + String(elapsedMillis));
  });

}
