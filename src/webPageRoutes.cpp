#include "func_webServerHandler.h"
#include "webServerRoutes.h"
#include "updateRuntimeState.h"
#include <WiFi.h>

void registerPageRoutes() {

  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    request->redirect("/index.html");
  });

  httpServer.on("/", HTTP_HEAD, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    request->redirect("/index.html");
  });

  httpServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = readFile2(LittleFS, "/index.html");
    //content.replace("%header%", String(readFile2(LittleFS, "/header.html")));
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));

    if (content == "- failed to open file for reading"){
      request->redirect("/initialsetup.html");
    } else {
      request->send(200, "text/html", content);
    }
  });

  httpServer.on("/index.html", HTTP_HEAD, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    if (LittleFS.exists("/index.html")) {
      request->send(200, "text/html", "");
    } else {
      request->redirect("/initialsetup.html");
    }
  });

  httpServer.on("/initialsetup.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = renderInitialSetupPage(initialsetup_html);
    request->send(200, "text/html", content);
  });

  httpServer.on("/gettable", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = "";
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));
    content = getMyDirAsString(LittleFS, "/", 0);
    //request->send(200, "text/html", content);
    request->send(200, "text/plain", content);
  });

  httpServer.on("/filesystem.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = renderInitialSetupPage(
      LittleFS.exists("/filesystem.html")
        ? readFile2(LittleFS, "/filesystem.html")
        : String(initialsetup_html)
    );
    content.replace("%wificonfig%", "");
    request->send(200, "text/html", content);
  });

  httpServer.on("/filesystem", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/filesystem.html");
  });

  httpServer.on("/sensorv.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = readFile2(LittleFS, "/sensorv.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));
    content.replace("%csrfToken%", getCsrfToken());

    String envSensorString = renderEnvironmentSensorMarkup();
    content.replace("%envSensorString%", String(envSensorString));

    //httpServer.sendHeader("Cache-Control", "max-age=600");
    //httpServer.send(200, "text/html", content);
    request->send(200, "text/html", content);
  });

  httpServer.on("/lora.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = readFile2(LittleFS, "/lora.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));
    content.replace("%csrfToken%", getCsrfToken());
    request->send(200, "text/html", content);
  });

  httpServer.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String inputMessage;
    JsonDocument data;

    if (request->hasParam("data")) {
      int paramsNr = request->params();

      for(int i=0;i<paramsNr;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "data") {
          inputMessage = p->value();
          if (inputMessage == "mainPowerOn" || inputMessage == "alarm1") {
            DebugPrintln(3, "getdata param = mainPowerOn");
            data["mainPowerOn"] = mainPowerOn;
            data["alarm1"] = mainPowerOn;
          }
          if (inputMessage == "Tank1") {
            data["Tank1"] = String(tank1p);
            data["Tank1adc"] = String(tank1adc);
          }
          if (inputMessage == "Battery") {
            if (String(actconf.envSensor) == "VEdirect-Read") {
              data["BatteryVoltage"] = String(vedirectVoltage, 3);
              data["BatteryAdc"] = "";
              data["BatteryCapacity"] = String(capacity, 0);
              data["CalibrationAvailable"] = false;
              data["CalibrationMessage"] = "Battery calibration for the analog input is not available while VEdirect-Read is active.";
            } else {
              configData batteryCalibrationConfig = actconf;
              const bool calibrationWasSanitized = sanitizeBatteryCalibration(batteryCalibrationConfig, defconf);
              const uint16_t rawBatteryAdc = samplePageBatteryAdcRaw(size_t(max(1, actconf.vaverage)));
              const float liveBatteryVoltage = calculateBatteryVoltageFromAdc(batteryCalibrationConfig, rawBatteryAdc);
              const float liveBatteryCapacity = constrain((liveBatteryVoltage * 100 / 2.2) - 477.27, 0.0f, 100.0f);
              data["BatteryVoltage"] = String(liveBatteryVoltage, 3);
              data["BatteryAdc"] = String(rawBatteryAdc);
              data["BatteryCapacity"] = String(liveBatteryCapacity, 0);
              data["CalibrationAvailable"] = rawBatteryAdc > 0;
              if (rawBatteryAdc > 0) {
                data["CalibrationMessage"] = calibrationWasSanitized
                  ? "Stored battery calibration looked unsafe. Safe defaults were used for this live reading. Save settings to persist the corrected values."
                  : "Live battery calibration values read successfully.";
              } else {
                data["CalibrationMessage"] = "Battery ADC returned 0. Check the wiring or sensor source before calibrating.";
              }
            }
          }
        }
      }
      data["data"] = true;
    } else {
      data["ping"] = true;
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  });

  httpServer.on("/savesettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(405, "application/json", "{\"status\":\"error\",\"message\":\"Use POST for saving settings.\"}");
  });
  httpServer.on("/savesettings", HTTP_POST, handleSettingsPageSave);

  httpServer.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    request->send(LittleFS, "/settings.html", String(), false, processSettingsPageTemplate);
  });

  httpServer.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send page
    String content = "";
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    content = readFile2(LittleFS, "/restart.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));
    content.replace("%csrfToken%", getCsrfToken());

    request->send(200, "text/html", content);
  });

  httpServer.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }

    DebugPrintln(3, "Restart requested via web interface");
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Device restart requested.\"}");
    reboot = true;
  });

  httpServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(405, "application/json", "{\"status\":\"error\",\"message\":\"Use POST for restart.\"}");
  });

  httpServer.on("/firmware.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = "";
    content = readFile2(LittleFS, "/firmware.html");
    content.replace("%mdsOtaUrl%", escapePageHtml(String(actconf.mdsOtaUrl)));
    content.replace("%mdsOtaSecretStatus%", strlen(actconf.mdsOtaSecret) > 0 ? "configured" : "not configured");

    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%csrfToken%", getCsrfToken());
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));

    request->send(200, "text/html", content);
  });

  httpServer.on("/startRemoteUpdate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireStrongPagePassword(request)) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      request->send(503, "application/json", createPageOtaResponse("error", "No Wi-Fi connection available for remote update.", false, false, false));
      return;
    }

    const RemoteOtaSnapshot ota = getRemoteOtaSnapshot();
    if (ota.pending || ota.inProgress) {
      request->send(409, "application/json", createPageOtaResponse("error", "A remote OTA update is already running.", false, false, false));
      return;
    }

    String source = request->hasParam("source", true) ? request->getParam("source", true)->value() : "";
    source.toLowerCase();
    const bool forceReinstall = request->hasParam("force", true) &&
      (request->getParam("force", true)->value() == "1" ||
       request->getParam("force", true)->value() == "true" ||
       request->getParam("force", true)->value() == "yes");

    String queueError;
    if (!queuePageRemoteOtaUpdate(source, forceReinstall, queueError)) {
      request->send(409, "application/json", createPageOtaResponse("error", queueError, false, false, false));
      return;
    }

    request->send(202, "application/json", createPageOtaResponse("queued", "Remote firmware check queued on device.", false, false, true));
  });

  httpServer.on("/devinfo.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    notePageRequestActivity();
    String content = readFile2(LittleFS, "/devinfo.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapePageHtml(String(actconf.devname)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%license%", String(actconf.license));
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));
    content.replace("%hname%", String(hname));
    String mdnsname = "";
      if(actconf.mDNS == 1){
        mdnsname = String(hname) + ".local";
      }
      else{
        mdnsname =F( "not activ");
      }
    content.replace("%mdnsname%", mdnsname);
    content.replace("%sssid%", escapePageHtml(String(actconf.sssid)));
    content.replace("%softAPIP%", WiFi.softAPIP().toString());
    content.replace("%WiFichannel%", String(WiFi.channel()));
    content.replace("%cssid1%", escapePageHtml(String(actconf.cssid1)));
    content.replace("%cssid2%", escapePageHtml(String(actconf.cssid2)));
    content.replace("%cssid3%", escapePageHtml(String(actconf.cssid3)));
    content.replace("%localIP%", WiFi.localIP().toString());
    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    content.replace("%devaddr%", mystring);

    String envSensorBME280 = "";
    if (String(actconf.envSensor) == "BME280") {
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Temperature</td>";
      envSensorBME280 += "<td><input id='airtemp' type='text' name='airtemp' size='15' value='0'></td>";
      envSensorBME280 += "<td>[<data id='atunit'></data>]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Pressure</td>";
      envSensorBME280 += "<td><input id='pressure' type='text' name='pressure' size='15' value='0'></td>";
      envSensorBME280 += "<td>[mbar]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Humidity</td>";
      envSensorBME280 += "<td><input id='humidity' type='text' name='humidity' size='15' value='0'></td>";
      envSensorBME280 += "<td>[%]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Dewpoint</td>";
      envSensorBME280 += "<td><input id='dewpoint' type='text' name='dewpoint' size='15' value='0'></td>";
      envSensorBME280 += "<td>[<data id='dpunit'></data>]</td>";
    envSensorBME280 += "</tr>";
  }
    content.replace("%envSensorBME280%", envSensorBME280);
    
    request->send(200, "text/html", content);
  });

}
