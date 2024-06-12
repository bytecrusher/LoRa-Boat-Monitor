#include "func_webServerHandler.h"

#include <AsyncElegantOTA.h>    // OTA lib

//const int relayPin = 25;      // Pin GPIO25, Relay is high activ

const char logout_html2[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>
    <body>
      <p>Logged out or <a href="/">return to homepage</a>.</p>
      <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
    </body>
  </html>
)rawliteral";

void WebServerHandler()
{
  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Read all received get arguments and save in a array
    int num = request->args();
    String vname[num];
    String value[num];
    for (int i = 0; i < num; i++) {
      vname[i] = request->argName(i);
      value[i] = request->arg(i);
      // Check the return value from Restart web page
      if(vname[i] == "restart" &&  value[i] == "1"){
        resetESP = 1;
      }
      else{
        resetESP = 0;
      }
    }
    request->redirect("/index.html");

    // Restart routine
    if(resetESP == 1){
      //delay(3000); // Waiting time for system restart
      resetESP = 0;
      // Restart the ESP32
      ESP.restart();
    }
  });

  httpServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/index.html");
    //content.replace("%header%", String(readFile2(LittleFS, "/header.html")));
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));

    if (content == "- failed to open file for reading"){
      request->redirect("/initialsetup.html");
    } else {
      request->send(200, "text/html", content);
    }
  });

  httpServer.on("/initialsetup.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = initialsetup_html;
    content.replace("%devname%", String(actconf.devname));
    content.replace("%crights%", String(actconf.crights));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%wificonfig%", wificonfig_html);
    content.replace("%cssid1%", String(actconf.cssid1));
    content.replace("%cpassword1%", String(actconf.cpassword1));
    content.replace("%cssid2%", String(actconf.cssid2));
    content.replace("%cpassword2%", String(actconf.cpassword2));
    content.replace("%cssid3%", String(actconf.cssid3));
    content.replace("%cpassword3%", String(actconf.cpassword3));
    content.replace("%quality%", String(int(quality)));
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));

    content.replace("%FREESPIFFS%", humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes())));
    content.replace("%USEDSPIFFS%", humanReadableSize(LittleFS.usedBytes()));
    content.replace("%TOTALSPIFFS%", humanReadableSize(LittleFS.totalBytes()));
    content.replace("%USEDSPIFFSvalue%", String(LittleFS.usedBytes()));
    content.replace("%TOTALSPIFFSvalue%", String(LittleFS.totalBytes()));

    request->send(200, "text/html", content);
  });

  httpServer.on("/gettable", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = "";
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));
    content = getMyDirAsString(LittleFS, "/", 0);
    //request->send(200, "text/html", content);
    request->send(200, "text/plain", content);
  });

  httpServer.on("/filesystem.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        request->requestAuthentication();
      }
    }

    String content = "";
    content = initialsetup_html;
    content.replace("%devname%", String(actconf.devname));
    content.replace("%crights%", String(actconf.crights));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%cssid1%", String(actconf.cssid1));
    content.replace("%cpassword1%", String(actconf.cpassword1));
    content.replace("%cssid2%", String(actconf.cssid2));
    content.replace("%cpassword2%", String(actconf.cpassword2));
    content.replace("%cssid3%", String(actconf.cssid3));
    content.replace("%cpassword3%", String(actconf.cpassword3));
    content.replace("%quality%", String(int(quality)));

    content.replace("%wificonfig%", "");
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));

    content.replace("%FREESPIFFS%", humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes())));
    content.replace("%USEDSPIFFS%", humanReadableSize(LittleFS.usedBytes()));
    content.replace("%TOTALSPIFFS%", humanReadableSize(LittleFS.totalBytes()));
    content.replace("%USEDSPIFFSvalue%", String(LittleFS.usedBytes()));
    content.replace("%TOTALSPIFFSvalue%", String(LittleFS.totalBytes()));

    request->send(200, "text/html", content);
  });

  httpServer.on("/sensorv.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/sensorv.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));

    String envSensorString = "";
    if (String(actconf.envSensor) == "BME280") {
    envSensorString +=F( "<h3>Environment <blink><data id='info'></data></blink>");
    envSensorString +=F( "</h3>");
    envSensorString +=F( "<FONT SIZE='4'>");
    envSensorString +=F( "<table>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
    envSensorString +=F( "<line x1='10' y1='9' x2='14' y2='9' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");

    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Temp:</td>");
    envSensorString +=F( "<td><data id='airtemp'></data><data id='atunit'></data></td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-wind' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M5 8h8.5a2.5 2.5 0 1 0 -2.34 -3.24' />");
    envSensorString +=F( "<path d='M3 12h15.5a2.5 2.5 0 1 1 -2.34 3.24' />");
    envSensorString +=F( "<path d='M4 16h5.5a2.5 2.5 0 1 1 -2.34 3.24' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Press:</td>");
    envSensorString +=F( "<td><data id='pressure'></data>mbar</td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-droplet' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M6.8 11a6 6 0 1 0 10.396 0l-5.197 -8l-5.2 8z' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Hum:</td>");
    envSensorString +=F( "<td><data id='humidity'></data>%</td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
    envSensorString +=F( "<line x1='10' y1='9' x2='14' y2='9' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Dew:</td>");
    envSensorString +=F( "<td><data id='dewpoint'></data><data id='dunit'></data></td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "</table>");
    envSensorString +=F( "</FONT><br>");
    envSensorString +=F( "<hr align='left'>");
    }
    content.replace("%envSensorString%", String(envSensorString));

    //httpServer.sendHeader("Cache-Control", "max-age=600");
    //httpServer.send(200, "text/html", content);
    request->send(200, "text/html", content);
  });

  httpServer.on("/lora.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/lora.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));
    request->send(200, "text/html", content);
  });

  httpServer.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    StaticJsonDocument<100> data;

    if (request->hasParam("data")) {
      int paramsNr = request->params();

      for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "data") {
          inputMessage = p->value();
          if (inputMessage == "alarm1") {
            DebugPrintln(3, "getdata param = alarm1");
            data["alarm1"] = alarm1;
          } if (inputMessage == "Tank1") {
            //data["Tank1"] = tank1;
            //data["Tank1adc"] = tank1adc;
            data["Tank1"] = String(tank1);
            data["Tank1adc"] = String(tank1adc);
            //json_Device["Device"]["MeasuringValues"]["Tank1adc"]["Value"] = String(tank1adc);
          } //else if (inputMessage == "ping") {
            //data["ping"] = true;
          //}
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
    // Read all received get arguments and save in a array
    int num = request->args();
    String vname[num];
    String value[num];
    for (int i = 0; i < num; i++) {
      vname[i] = request->argName(i);
      value[i] = request->arg(i);  
    } 
    String hash = "";
  //  bool reboot = false;

    // Check new settings and save it in configuration
    for (int i = 0; i < num; i++)
    {
      // Passwort Settings
      //******************
      if(vname[i] == "password"){
        hash = value[i];
      }
      if (vname[i] == "usepassword") {
        actconf.crypt = toInteger(value[i]);
      }
      if (vname[i] == "pagepasswd") {
        value[i].toCharArray(actconf.password, 31);
      }
      // Display Settings
      //*****************
      if (vname[i] == "isize") {
        actconf.instrumentSize = toInteger(value[i]);
      }
      // Network Settings
      //*****************
      if (vname[i] == "cssid1") {
        value[i].toCharArray(actconf.cssid1, 31);
      }
      if (vname[i] == "cpasswd1") {
        value[i].toCharArray(actconf.cpassword1, 31);
      }
      if (vname[i] == "cssid2") {
        value[i].toCharArray(actconf.cssid2, 31);
      }
      if (vname[i] == "cpasswd2") {
        value[i].toCharArray(actconf.cpassword2, 31);
      }
      if (vname[i] == "cssid3") {
        value[i].toCharArray(actconf.cssid3, 31);
      }
      if (vname[i] == "cpasswd3") {
        value[i].toCharArray(actconf.cpassword3, 31);
      }
      if (vname[i] == "timeout") {
        actconf.timeout = toInteger(value[i]);
      }
      if (vname[i] == "sssid") {
        value[i].toCharArray(actconf.sssid, 31);
      }
      if (vname[i] == "spasswd") {
        value[i].toCharArray(actconf.spassword, 31);
      }
      if (vname[i] == "apchannel") {
        actconf.apchannel = toInteger(value[i]);
      }
      if (vname[i] == "firmwareUpdateUrl") {
        value[i].toCharArray(actconf.firmwareUpdateUrl, 51);
      }
      if (vname[i] == "servermode") {
        actconf.serverMode = toInteger(value[i]);
      }
      if (vname[i] == "mdnsservice") {
        actconf.mDNS = toInteger(value[i]);
      }
      if (vname[i] == "SendDataViaWifi") {
        //actconf.SendDataViaWifi = toInteger(value[i]);
        value[i].toCharArray(actconf.SendDataViaWifi, 4);
      }
      if (vname[i] == "MdsUrl") {
        value[i].toCharArray(actconf.MdsUrl, 100);
      }
      if (vname[i] == "MdsApiKey") {
        value[i].toCharArray(actconf.MdsApiKey, 30);
      }
      // LoRa Settings
      //**************
      if (vname[i] == "devaddr") {
        char hexstring[8];
        value[i].toCharArray(hexstring, 9);
        actconf.devaddr = HexToInt(hexstring);
      }
      if (vname[i] == "nskey") {
        char mystring[33];
        char hexstring[2];
        value[i].toCharArray(mystring, 33);
        DebugPrintln(3, mystring);
        for (int j = 0; j < 16; j++){
          hexstring[0] = mystring[j*2];
          hexstring[1] = mystring[j*2+1];
          actconf.nskey[j] = HexToInt(hexstring);
          DebugPrint(3, j);
          DebugPrint(3, " ");
          DebugPrint(3, HexToInt(hexstring));
          DebugPrint(3, " ");
          DebugPrintln(3, hexstring);
        }
      }
      if (vname[i] == "appkey") {
        char mystring[33];
        char hexstring[2];
        value[i].toCharArray(mystring, 33);
        DebugPrintln(3, mystring);
        for (int j = 0; j < 16; j++){
          hexstring[0] = mystring[j*2];
          hexstring[1] = mystring[j*2+1];
          actconf.appkey[j] = HexToInt(hexstring);
          DebugPrint(3, j);
          DebugPrint(3, " ");
          DebugPrint(3, HexToInt(hexstring));
          DebugPrint(3, " ");
          DebugPrintln(3, hexstring);
        }
      } 
      if (vname[i] == "lorafrequency") {
        value[i].toCharArray(actconf.lorafrequency, 31);
      }
      if (vname[i] == "lchannel") {
        actconf.lchannel = toInteger(value[i]);
        // Enable the used LoRa channels
        //setChannel(actconf.lchannel);
      }
      if (vname[i] == "dynsf") {
        actconf.dynsf = toInteger(value[i]);
      }
      if (vname[i] == "spreadf") {
        actconf.spreadf = toInteger(value[i]);
        // Set spreading factor and dynamic SF
        //setSF(slot, actconf.spreadf, actconf.dynsf);
      }
      if (vname[i] == "tinterval") {
        if(actconf.tinterval != toInteger(value[i])){
          actconf.tinterval = toInteger(value[i]);
          // Set LoRa transmit interval
          TX_INTERVAL = actconf.tinterval * 60; // Send interval * 30s
          reboot = true; // Need a reboot
        }
      }
      //if (vname[i] == "sendlora") {
      //  actconf.sendlora = toInteger(value[i]);
      //}
      if (vname[i] == "relay") {
        actconf.relay = toInteger(value[i]);
        if(actconf.relay == 0){
          digitalWrite(relayPin, LOW);
          relaytimer = 0;
        }
        else{
          digitalWrite(relayPin, HIGH);
          relaytimer = 288;  // 288 x 5min = 1440min
        }
      }
      // DeviceSettings
      //*************** 
      if (vname[i] == "debugmode") {
        actconf.debug = toInteger(value[i]);
      }
      if (vname[i] == "serspeed") {
        actconf.serspeed = toInteger(value[i]);
      }

      if (vname[i] == "WebSerialDebug") {
        actconf.WebSerialDebug = toInteger(value[i]);
      }

      if (vname[i] == "deviceid") {
        actconf.deviceID = toInteger(value[i]);
      }
      if (vname[i] == "senddata") {
        actconf.senddata = toInteger(value[i]);
      }    
      if (vname[i] == "vaverage") {
        actconf.vaverage = toInteger(value[i]);
      }
      if (vname[i] == "t1average") {
        actconf.t1average = toInteger(value[i]);
      }
      if (vname[i] == "t2average") {
        actconf.t2average = toInteger(value[i]);
      }   
      if (vname[i] == "tstype") {
        value[i].toCharArray(actconf.tempSensorType, 10);
      }
      if (vname[i] == "tempunit") {
        value[i].toCharArray(actconf.tempUnit, 2);
      }
      if (vname[i] == "envSensor") {
        value[i].toCharArray(actconf.envSensor, 20);
      }
      if (vname[i] == "standbyMode") {
        // Check if Alarm is High (?), to prevent the user from locking out.
        //String(alarm1)
        value[i].toCharArray(actconf.standbyMode, 4);
      }
      if (vname[i] == "standbySleepDuration") {
        //value[i].toInteger(actconf.standbySleepDuration);
        actconf.standbySleepDuration = toInteger(value[i]);
      }
      if (vname[i] == "loraOperationMode") {
        value[i].toCharArray(actconf.loraOperationMode, 8);
      }
      if (vname[i] == "WifiStandbyMode") {
        value[i].toCharArray(actconf.WifiStandbyMode, 8);
      }
      // Calibration Settings
      //*********************    
      if (vname[i] == "a1vslope") {
        actconf.a1vslope = toFloat(value[i]);
      }
      if (vname[i] == "a2vslope") {
        actconf.a2vslope = toFloat(value[i]);
      }
      if (vname[i] == "voffset") {
        actconf.voffset = toFloat(value[i]);
      }
      if (vname[i] == "a1t1slope") {
        actconf.a1t1slope = toFloat(value[i]);
      }
      if (vname[i] == "a2t1slope") {
        actconf.a2t1slope = toFloat(value[i]);
      }
      if (vname[i] == "t1offset") {
        actconf.t1offset = toFloat(value[i]);
      }
      if (vname[i] == "a1t2slope") {
        actconf.a1t2slope = toFloat(value[i]);
      }
      if (vname[i] == "a2t2slope") {
        actconf.a2t2slope = toFloat(value[i]);
      }
      if (vname[i] == "t2offset") {
        actconf.t2offset = toFloat(value[i]);
      }
      if (vname[i] == "cssStyle") {
        actconf.cssStyle = toFloat(value[i]);
      }
      if (vname[i] == "OledDisplayRotation") {
        actconf.OledDisplayRotation = toFloat(value[i]);
      }
    }
    // Save the settings if the number of return values is greater 0
    if(num > 0) {  
      saveEEPROMConfig(actconf);      // Save the new settings in EEPROM
      DebugPrintln(3, "New settings saved");
    }
    // Debug info
    DebugPrintln(3, "Send settings.html");
    // Reboot when TX_INTERVAL is changed
    if(reboot){
      DebugPrintln(3, "Reboot");
      ESP.restart(); // Restart ESP32
    }
    request->redirect("/settings.html");
    //request->send(200, "text/plain", "OK");
  });

  httpServer.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send page
    String content = "";

    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    // Generate a new transaction ID
    transID();
    content = readFile2(LittleFS, "/settings.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));
    content.replace("%cssid1%", String(actconf.cssid1));
    content.replace("%cpassword1%", String(actconf.cpassword1));
    content.replace("%cssid2%", String(actconf.cssid2));
    content.replace("%cpassword2%", String(actconf.cpassword2));
    content.replace("%cssid3%", String(actconf.cssid3));
    content.replace("%cpassword3%", String(actconf.cpassword3));
    content.replace("%username%", String(actconf.username));
    content.replace("%password%", String(actconf.password));
    content.replace("%SendDataViaWifi%", String(getindex(SendDataViaWifi, String(actconf.SendDataViaWifi))));
    content.replace("%firmwareUpdateUrl%", String(actconf.firmwareUpdateUrl));

    content.replace("%MdsUrl%", String(actconf.MdsUrl));
    content.replace("%MdsApiKey%", String(actconf.MdsApiKey));

    content.replace("%hostname%", String(actconf.hostname));
    content.replace("%sssid%", String(actconf.sssid));
    content.replace("%spassword%", String(actconf.spassword));

    content.replace("%crypt%", String(getindex(usepassword, String(actconf.crypt))));
    content.replace("%instrumentSize%", String(getindex(isize, String(actconf.instrumentSize))));

    content.replace("%timeout%", String(getindex(timeout, String(actconf.timeout))));
    content.replace("%apchannel%", String(getindex(apchannel, String(actconf.apchannel))));
    content.replace("%serverMode%", String(getindex(servermode, String(actconf.serverMode))));
    content.replace("%mDNS%", String(getindex(mdnsservice, String(actconf.mDNS))));

    content.replace("%lorafrequency%", String(getindex(lorafrequencys, String(actconf.lorafrequency))));
    content.replace("%lchannel%", String(getindex(lchannel, String(actconf.lchannel))));
    content.replace("%spreadf%", String(getindex(spreadf, String(actconf.spreadf))));
    content.replace("%dynsf%", String(getindex(dynsf, String(actconf.dynsf))));
    content.replace("%tinterval%", String(actconf.tinterval));
    content.replace("%relay%", String(getindex(relay, String(actconf.relay))));

    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    String devaddr = mystring;
    content.replace("%devaddr%", String(devaddr));

    String nskey = "";
    //DebugPrintln(3, "nskey: ");
    for (int i = 0; i < 16; i++){
      String mystring = String(actconf.nskey[i], HEX);
      //DebugPrint(3, String(mystring));
      if(mystring.length() == 1){
        mystring = "0" + mystring;
      }
      mystring.toUpperCase();
      nskey += mystring;
    }
    //DebugPrint(3, ", komplett: ");
    //DebugPrintln(3, String(nskey));
    content.replace("%nskey%", String(nskey));

    String appkey = "";
    for (int i = 0; i < 16; i++){
      String mystring = String(actconf.appkey[i], HEX);
      if(mystring.length() == 1){
        mystring = "0" + mystring;
      }
      mystring.toUpperCase();
      appkey += mystring;
    }
    content.replace("%appkey%", String(appkey));

    char a1t1slope[20];
    sprintf(a1t1slope, "%.5f", actconf.a1t1slope);
    content.replace("%a1t1slope%", String(a1t1slope));

    char a2t1slope[20];
    sprintf(a2t1slope, "%.5f", actconf.a2t1slope);
    content.replace("%a2t1slope%", String(a2t1slope));

    char t1offset[20];
    sprintf(t1offset, "%.5f", actconf.t1offset);
    content.replace("%t1offset%", String(t1offset));

    char a2t2slope[20];
    sprintf(a2t2slope, "%.5f", actconf.a2t2slope);
    content.replace("%a2t2slope%", String(a2t2slope));

    char a1t2slope[20];
    sprintf(a1t2slope, "%.5f", actconf.a1t2slope);
    content.replace("%a1t2slope%", String(a1t2slope));

    char t2offset[20];
    sprintf(t2offset, "%.5f", actconf.t2offset);
    content.replace("%t2offset%", String(t2offset));

    content.replace("%userpassword%", String(cryptPassword(String(actconf.password))));

    char voffset[20];
    sprintf(voffset, "%.5f", actconf.voffset);
    content.replace("%voffset%", String(voffset));

    char a2vslope[20];
    sprintf(a2vslope, "%.5f", actconf.a2vslope);
    content.replace("%a2vslope%", String(a2vslope));

    char a1vslope[20];
    sprintf(a1vslope, "%.5f", actconf.a1vslope);
    content.replace("%a1vslope%", String(a1vslope));

    content.replace("%debug%", String(getindex(debugmode, String(actconf.debug))));
    content.replace("%serspeed%", String(getindex(serspeed, String(actconf.serspeed))));
    content.replace("%WebSerialDebug%", String(getindex(WebSerialDebug, String(actconf.WebSerialDebug))));

    content.replace("%deviceID%", String(getindex(deviceid, String(actconf.deviceID))));
    //content.replace("%sendlora%", String(getindex(sendlora, String(actconf.sendlora))));
    content.replace("%senddata%", String(getindex(senddata, String(actconf.senddata))));
    content.replace("%vaverage%", String(getindex(vaverage, String(actconf.vaverage))));
    content.replace("%t1average%", String(getindex(t1average, String(actconf.t1average))));
    content.replace("%t2average%", String(getindex(t2average, String(actconf.t2average))));
    content.replace("%tempSensorType%", String(getindex(tstype, String(actconf.tempSensorType))));
    content.replace("%tempUnit%", String(getindex(tempunits, String(actconf.tempUnit))));
    content.replace("%envSensor%", String(getindex(envSensor, String(actconf.envSensor))));
    content.replace("%standbyMode%", String(getindex(standbyMode, String(actconf.standbyMode))));
    content.replace("%standbySleepDuration%", String(actconf.standbySleepDuration));
    content.replace("%loraOperationMode%", String(getindex(loraOperationMode, String(actconf.loraOperationMode))));
    content.replace("%WifiStandbyMode%", String(getindex(WifiStandbyMode, String(actconf.WifiStandbyMode))));
    content.replace("%cssStyle%", String(getindex(cssStyle, String(actconf.cssStyle))));
    content.replace("%OledDisplayRotation%", String(getindex(OledDisplayRotation, String(actconf.OledDisplayRotation))));

    request->send(200, "text/html", content);
  });

  httpServer.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send page
    String content = "";
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    // Generate a new transaction ID
    transID();
    content = readFile2(LittleFS, "/restart.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));

    request->send(200, "text/html", content);
  });

  httpServer.on("/firmware.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        request->requestAuthentication();
      }
    }

    String content = "";
    transID();
    content = readFile2(LittleFS, "/firmware.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));

    request->send(200, "text/html", content);
  });

  httpServer.on("/devinfo.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/devinfo.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", String(actconf.devname));
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
    content.replace("%sssid%", String(actconf.sssid));
    content.replace("%softAPIP%", WiFi.softAPIP().toString());
    content.replace("%WiFichannel%", String(WiFi.channel()));
    content.replace("%cssid1%", String(actconf.cssid1));
    content.replace("%cssid2%", String(actconf.cssid2));
    content.replace("%cssid3%", String(actconf.cssid3));
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

  httpServer.serveStatic("/favicon.ico", LittleFS, "/favicon.ico").setCacheControl("max-age=600");

  httpServer.serveStatic("/settings.js", LittleFS, "/settings.js").setCacheControl("max-age=600");

  httpServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    long int t1 = millis();
    String content = "";
    String cssfilename = "";
    // Style activation
    switch (actconf.cssStyle) {
    case 0:
      cssfilename = "/css_black.css";
      break;
    case 1:
      cssfilename = "/css_red.css";
      break;
    case 2:
      cssfilename = "/css_white.css";
      break;  
    default:
      cssfilename = "/css_black.css";
    }
    if (LittleFS.exists(cssfilename)) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, cssfilename, "text/css");
      response->addHeader("Cache-Control", "max-age=100");
      request->send(response);
    } else {
      request->send(404, "text/plain", "The content you are looking for was not found.");
    }
    long int t2 = millis();
    DebugPrint(3, "Time taken by the task: ");
    DebugPrint(3, String(t2-t1));
    DebugPrintln(3, " milliseconds");
  });

  httpServer.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/app.js");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/javascript", content);
    response->addHeader("Cache-Control", "max-age=600");
    request->send(response);
  });

  httpServer.on("/staticdata.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument json_Device(8048);
    json_Device["Device"]["Type"] = String(actconf.devname);
    json_Device["Device"]["CopyRights"] = String(actconf.crights);
    json_Device["Device"]["FirmwareVersion"] = String(actconf.fversion);
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
    json_Device["Device"]["NetworkParameter"]["MdsApiKey"] = String(actconf.MdsApiKey);

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
    //json_Device["Device"]["DeviceSettings"]["TempSensorType"] = String(actconf.tempSensorType);
    //json_Device["Device"]["DeviceSettings"]["TempUnit"] = String(actconf.tempUnit);
    json_Device["Device"]["DeviceSettings"]["envSensor"] = String(actconf.envSensor);
    json_Device["Device"]["DeviceSettings"]["standbyMode"] = String(actconf.standbyMode);

    String stringjsondata = "";
    serializeJson(json_Device, stringjsondata);

    request->send(200, "application/json", stringjsondata);
  });

  httpServer.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    //unsigned long previousMillis = 0;
    //unsigned long elapsedMillis = 0;
    //previousMillis = millis();
  
    DynamicJsonDocument json_Device(8048);
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

    json_Device["Device"]["MeasuringValues"]["Alarm"]["Value"] = String(alarm1);
    json_Device["Device"]["MeasuringValues"]["Alarm"]["Unit"] = "bin";  // TODO: bring into staticdata.json

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

    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Value"] = "%vedirectVoltage%";
    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Value"] = "%vedirectCurrent%";
    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Unit"] = "A";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Value"] = "%vedirectTemp%";
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
    String timestr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
    json_Device["Device"]["MeasuringValues"]["Time"]["Value"] = String(timestr);
    json_Device["Device"]["MeasuringValues"]["Time"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    String zday = firstzero(day);
    String zmonth = firstzero(month);
    String zyear = firstzero(year);
    String datestr = "\"" + String(zday) + "." + String(zmonth) + "." + String(zyear) + "\"";
    json_Device["Device"]["MeasuringValues"]["Date"]["Value"] = String(datestr);
    json_Device["Device"]["MeasuringValues"]["Date"]["Unit"] = "GTM";  // TODO: bring into staticdata.json

    String sunrisestr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Value"] = String(sunrisestr);
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    String sunsetstr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Value"] = String(sunsetstr);
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

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

  httpServer.on("/reseteeprom", HTTP_GET, [](AsyncWebServerRequest *request) {
    saveEEPROMConfig(defconf);
    request->send(200, "text/javascript", "ok, EEPROM erased.");
  });

  // Use no cache because the js was permanently modifyed (transaction ID)
  httpServer.on("/MD5.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/md5.js");
    content.replace("%transactionID%", String(transactionID));

    request->send(200, "text/javascript", content);
  });

  httpServer.serveStatic("/md5.min.js", LittleFS, "/md5.min.js").setCacheControl("max-age=600");

  httpServer.serveStatic("/md5.min.js.map", LittleFS, "/md5.min.js.map").setCacheControl("max-age=600");

  httpServer.serveStatic("/gauge.min.js", LittleFS, "/gauge.min.js").setCacheControl("max-age=600");

  // Firmware update
  AsyncElegantOTA.begin(&httpServer);    // Start ElegantOTA

  /*httpServer.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
  //httpServer.on("/update", HTTP_POST, []() {
    //httpServer.sendHeader("Connection", "close");
    //httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, [](AsyncWebServerRequest *request) {*/
  // Update routine
  /*HTTPUpload& upload = httpServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        DebugPrint(3, "Update: " + String(upload.filename.c_str()) + "\n");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
  /*      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          DebugPrint(3, "Update Success: " + String(upload.totalSize) + "\nRebooting...\n");
        } else {
          Update.printError(Serial);
        }*/
      /*} */
  //});

  // run handleUpload function when any file is uploaded
  httpServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, handleUpload);

  /*handling uploading file */
  /*httpServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  //httpServer.on("/upload", HTTP_POST, [](){
    httpServer.sendHeader("Connection", "close");
  },[](){  
    HTTPUpload& upload = httpServer.upload();
      root = LittleFS.open((String("/") + upload.filename).c_str(), FILE_WRITE);
      if(!root){
        DebugPrintln(1, "- failed to open file for writing");
        return;
      }
    if(upload.status == UPLOAD_FILE_WRITE){
      if(root.write(upload.buf, upload.currentSize) != upload.currentSize){
        DebugPrintln(1, "- failed to write");
        return;
      }
    } else if(upload.status == UPLOAD_FILE_END){
      root.close();
      DebugPrintln(3, "UPLOAD_FILE_END");
    }
  });*/

  httpServer.on("/formatfs", HTTP_POST, [](AsyncWebServerRequest *request) {
    formatfs(LittleFS);
    request->send(200, "text/html", "done");
  });

  httpServer.on("/updatefiles", HTTP_POST, [](AsyncWebServerRequest *request) {
    runDownloadingFiles = true;
    request->send(200, "text/html", "done");
  });

  httpServer.on("/updatefilesstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    String test = (String)runDownloadingFiles;
    request->send(200, "text/html", test);
  });

  httpServer.onNotFound([](AsyncWebServerRequest *request){
    if (request->method() == HTTP_OPTIONS) 
    {
      request->send(200);
    } 
    else 
    {
      String content = readFile2(LittleFS, "/error.html");
      content.replace("%header%", getheader(actconf));
      content.replace("%devname%", String(actconf.devname));
      request->send(404, "text/html", content);
    }
  });

  httpServer.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  httpServer.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html2);
  });
    
}

