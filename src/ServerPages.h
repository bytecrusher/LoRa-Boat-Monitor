// Server pages
// Insert this library after server definition

/*httpServer.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
  String inputMessage1;
  String inputMessage2;
  // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
    inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
    inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
    digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
  }
  else {
    inputMessage1 = "No message sent";
    inputMessage2 = "No message sent";
  }
  Serial.print("GPIO: ");
  Serial.print(inputMessage1);
  Serial.print(" - Set to: ");
  Serial.println(inputMessage2);
  request->send(200, "text/plain", "OK");
});*/

// Route for root / web page
  /*server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });*/

  // Route for root index.html
  //httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //  request->send(LittleFS, "/main.html", "text/html");
  //});

httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/2", []() {
  // Read all received get arguments and save in a array
  //int num = httpServer.args();
  int num = request->args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    //vname[i] = httpServer.argName(i);
    vname[i] = request->argName(i);
    //value[i] = httpServer.arg(i);
    value[i] = request->arg(i);
    // Check the return value from Restart web page
    if(vname[i] == "restart" &&  value[i] == "1"){
      resetESP = 1;
    }
    else{
      resetESP = 0;
    }
  }

  String content = readFile2(LittleFS, "/main.html");
  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  if (content == "- failed to open file for reading"){
    content += handleRoot(LittleFS, "/", 0);
  }
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  //request->send(LittleFS, "/main.html", "text/html");
  request->send(200, "text/html", content);

  // Restart routine
  if(resetESP == 1){
    delay(3000); // Waiting time for system restart
    resetESP = 0;
    // Restart the ESP32
    ESP.restart();
  }
});

/*httpServer.on("/test.html", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/test.html", []() {
  Serial.print(ESP.getFreeHeap());
  Serial.print(ESP.getMaxAllocHeap());
  String content = readFile2(LittleFS, "/test.html");
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
  Serial.print(ESP.getFreeHeap());
  Serial.print(ESP.getMaxAllocHeap());
});*/

httpServer.on("/sensorv", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/sensorv", []() {
  // Send page
  //String content = Sensorv();
  String content = readFile2(LittleFS, "/sensorv.html");
  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));

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

httpServer.on("/lora", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/lora", []() {
  // Send page
  //String content = Lora();
  String content = readFile2(LittleFS, "/lora.html");

  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  //httpServer.sendHeader("Cache-Control", "max-age=600");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

httpServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/settings", []() {
  // Read all received get arguments and save in a array
  //int num = httpServer.args();
  int num = request->args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    //vname[i] = httpServer.argName(i);
    //value[i] = httpServer.arg(i);  
    vname[i] = request->argName(i);
    value[i] = request->arg(i);  
  } 
  // Send page
  //String content = Settings(num, vname, value);
  String content = "";

  String hash = "";
  bool reboot = false;
  
  // Print all received get arguments
  for (int i = 0; i < num; i++)
  {
    String out = vname[i] + " : " + value[i];
     DebugPrintln(3, out);
  }

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
    if (vname[i] == "cssid") {
      value[i].toCharArray(actconf.cssid, 31);
    }
    if (vname[i] == "cpasswd") {
      value[i].toCharArray(actconf.cpassword, 31);
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
    if (vname[i] == "servermode") {
      actconf.serverMode = toInteger(value[i]);
    }
    if (vname[i] == "mdnsservice") {
      actconf.mDNS = toInteger(value[i]);
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
      setChannel(actconf.lchannel);
    }
    if (vname[i] == "dynsf") {
      actconf.dynsf = toInteger(value[i]);
    }
    if (vname[i] == "spreadf") {
      actconf.spreadf = toInteger(value[i]);
      // Set spreading factor and dynamic SF
      setSF(slot, actconf.spreadf, actconf.dynsf);
    }
    if (vname[i] == "tinterval") {
      if(actconf.tinterval != toInteger(value[i])){
        actconf.tinterval = toInteger(value[i]);
        // Set LoRa transmit interval
        TX_INTERVAL = actconf.tinterval * 30; // Send interval * 30s
        reboot = true; // Need a reboot
      }
    }
    if (vname[i] == "sendlora") {
      actconf.sendlora = toInteger(value[i]);
    }
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
    if (vname[i] == "deviceid") {
      actconf.deviceID = toInteger(value[i]);
    }
    if (vname[i] == "devicetype") {
      value[i].toCharArray(actconf.deviceType, 10);
    }
    if (vname[i] == "sendlora") {
      actconf.sendlora = toInteger(value[i]);
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
      value[i].toCharArray(actconf.standbyMode, 4);
    }
    if (vname[i] == "loraStandbyMode") {
      value[i].toCharArray(actconf.loraStandbyMode, 8);
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
  // Check page password
 if(actconf.crypt == 1 && (hash.length() == 0 || hash != cryptPassword(String(actconf.password)))){
   // Generate a new transaction ID
   transID();
   content = readFile2(LittleFS, "/password.html");
   content.replace("%action%", "settings");
 } else{
   // Generate a new transaction ID
   transID();
    content = readFile2(LittleFS, "/settings.html");

    content.replace("%devname%", String(actconf.devname));
    content.replace("%crights%", String(actconf.crights));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%cssid%", WiFi.localIP().toString());
    content.replace("%cpassword%", String(actconf.cpassword));
    content.replace("%password%", String(actconf.password));
    content.replace("%quality%", String(int(quality)));

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
    content.replace("%tinterval%", String(getindex(dynsf, String(actconf.tinterval))));
    content.replace("%relay%", String(getindex(relay, String(actconf.relay))));

    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    String devaddr = mystring;
    content.replace("%devaddr%", String(devaddr));

    String nskey = "";
    for (int i = 0; i < 16; i++){
      String mystring = String(actconf.nskey[i], HEX);
      if(mystring.length() == 1){
        mystring = "0" + mystring;
      }
      mystring.toUpperCase();
      nskey = mystring;
    }
    content.replace("%nskey%", String(nskey));

    String appkey = "";
    for (int i = 0; i < 16; i++){
      String mystring = String(actconf.appkey[i], HEX);
      if(mystring.length() == 1){
        mystring = "0" + mystring;
      }
      mystring.toUpperCase();
      appkey = mystring;
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
    content.replace("%deviceID%", String(getindex(deviceid, String(actconf.deviceID))));
    content.replace("%sendlora%", String(getindex(sendlora, String(actconf.sendlora))));
    content.replace("%senddata%", String(getindex(senddata, String(actconf.senddata))));
    content.replace("%vaverage%", String(getindex(vaverage, String(actconf.vaverage))));
    content.replace("%t1average%", String(getindex(t1average, String(actconf.t1average))));
    content.replace("%t2average%", String(getindex(t2average, String(actconf.t2average))));
    content.replace("%tempSensorType%", String(getindex(tstype, String(actconf.tempSensorType))));
    content.replace("%tempUnit%", String(getindex(tempunits, String(actconf.tempUnit))));
    content.replace("%envSensor%", String(getindex(envSensor, String(actconf.envSensor))));
    content.replace("%standbyMode%", String(getindex(standbyMode, String(actconf.standbyMode))));
    content.replace("%loraStandbyMode%", String(getindex(loraStandbyMode, String(actconf.loraStandbyMode))));
  }
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

httpServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/restart", []() {
  // Read all received get arguments and save in a array
  //int num = httpServer.args();
  int num = request->args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    //vname[i] = httpServer.argName(i);
    //value[i] = httpServer.arg(i);
    vname[i] = request->argName(i);
    value[i] = request->arg(i);
  } 

  // Send page
  String hash = "";
 // Print all received get arguments
 for(int i = 0; i < num; i++)
  {
    String out = vname[i] + " : " + value[i];
    DebugPrintln(3, out);
    if(vname[i] == "password"){
      hash = value[i];
    }
  }
  String content = "";
 // Check page password
 if(actconf.crypt == 1 && (hash.length() == 0 || hash != cryptPassword(String(actconf.password)))){
   // Generate a new transaction ID
   transID();
    content = readFile2(LittleFS, "/password.html");
    content.replace("%action%", "restart");
     }
 else{
   // Generate a new transaction ID
   transID();
    content = readFile2(LittleFS, "/restart.html");
    content.replace("%devname%", String(actconf.devname));
    content.replace("%crights%", String(actconf.crights));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%quality%", String(wlanquality()));
 }

  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

httpServer.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/firmware", []() {
  // Read all received get arguments and save in a array
  //int num = httpServer.args();
  int num = request->args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    //vname[i] = httpServer.argName(i);
    //value[i] = httpServer.arg(i);  
    vname[i] = request->argName(i);
    value[i] = request->arg(i);  
  } 

  String hash = "";
  // Print all received get arguments
  for(int i = 0; i < num; i++)
  {
    String out = vname[i] + " : " + value[i];
    DebugPrintln(3, out);
    
    if(vname[i] == "password"){
      hash = value[i];
    }
  }

  String content = "";
  if(actconf.crypt == 1 && (hash.length() == 0 || hash != cryptPassword(String(actconf.password)))){
    // Generate a new transaction ID
    transID();
    content = readFile2(LittleFS, "/password.html");
    content.replace("%action%", "firmware");
  } else {
    transID();
    content = readFile2(LittleFS, "/firmware.html");
    content.replace("%devname%", String(actconf.devname));
    content.replace("%crights%", String(actconf.crights));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%wlanquality%", String(wlanquality()));
    content.replace("%license%", String(actconf.license));
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));
  }
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

httpServer.on("/devinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/devinfo", []() {
  String content = readFile2(LittleFS, "/devinfo.html");
  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  content.replace("%wlanquality%", String(wlanquality()));
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
  content.replace("%softAPIP%", WiFi.softAPIP().toString());
  content.replace("%WiFichannel%", String(WiFi.channel()));
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
  
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

httpServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/favicon.ico", []() {
  String content = readFile2(LittleFS, "/favicon.ico");
  //httpServer.sendHeader("Cache-Control", "max-age=600");
  //httpServer.send(200, "image/svg+xml", content);
  request->send(200, "image/svg+xml", content);
});

httpServer.on("/css", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/css", []() {
  String content = "";
   // Style activation
  switch (style) {
  case 0:
    // Night style red
    content = readFile2(LittleFS, "/css_red.css");
    break;
  case 1:
    // Day style black
    content = readFile2(LittleFS, "/css_black.css");
    break;
  case 2:
    // Day style white
    content = readFile2(LittleFS, "/css_white.css");
    break;  
  default:
    // Day style white
    content = readFile2(LittleFS, "/css_white.css");
  }
  //httpServer.sendHeader("Cache-Control", "max-age=1");
  //httpServer.send(200, "text/css", content);
  request->send(200, "text/css", content);
});

httpServer.on("/js", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/js", []() {
  String content = readFile2(LittleFS, "/js.js");
  //httpServer.sendHeader("Cache-Control", "max-age=600");
  //httpServer.send(200, "text/javascript", content);
  request->send(200, "text/javascript", content);
});

httpServer.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/json", []() {
  //unsigned long previousMillis = 0;
  //unsigned long elapsedMillis = 0;
  //previousMillis = millis();
 
  DynamicJsonDocument json_Device(8048);
  json_Device["Device"]["Type"] = String(actconf.devname);
  json_Device["Device"]["CopyRights"] = String(actconf.crights);
  json_Device["Device"]["FirmwareVersion"] = String(actconf.fversion);
  json_Device["Device"]["License"] = String(actconf.license);

  json_Device["Device"]["ESP32"]["SDKVersion"] = String(ESP.getSdkVersion());
  json_Device["Device"]["ESP32"]["ChipID"] = String(chipId);
  json_Device["Device"]["ESP32"]["CPUSpeed"]["Value"] = String(ESP.getCpuFreqMHz());
  json_Device["Device"]["ESP32"]["CPUSpeed"]["Unit"] = "MHz";

  json_Device["Device"]["ESP32"]["FreeHeapSize"]["Value"] = String(ESP.getFreeHeap());
  json_Device["Device"]["ESP32"]["FreeHeapSize"]["Unit"] = "Byte";

  json_Device["Device"]["NetworkParameter"]["WLANClientSSID"] = String(actconf.cssid);
  json_Device["Device"]["NetworkParameter"]["WLANClientIP"] = WiFi.localIP().toString();
  json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Value"] = String(fieldstrength);
  json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Unit"] = "dBm";

  json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Value"] = String(quality);
  json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Unit"] = "%";

  json_Device["Device"]["NetworkParameter"]["WLANServerSSID"] = String(actconf.sssid);
  json_Device["Device"]["NetworkParameter"]["WLANServerIP"] = WiFi.softAPIP().toString();
  json_Device["Device"]["NetworkParameter"]["ServerMode"] = String(actconf.serverMode);
  json_Device["Device"]["NetworkParameter"]["ServerHostName"] = String(actconf.hostname);

  json_Device["Device"]["LoRaSettings"]["DeviceAddress"] = String(actconf.devname);
  json_Device["Device"]["LoRaSettings"]["Frequency"] = String(actconf.lorafrequency);
  json_Device["Device"]["LoRaSettings"]["Channel"] = String(actconf.lchannel);
  json_Device["Device"]["LoRaSettings"]["ActualChannel"] = String(LMIC.txChnl);
  json_Device["Device"]["LoRaSettings"]["SpreadingFactor"] = String(actconf.spreadf);
  json_Device["Device"]["LoRaSettings"]["ActualSF"] = String(sf);
  json_Device["Device"]["LoRaSettings"]["DynamicSF"] = String(actconf.dynsf);
  json_Device["Device"]["LoRaSettings"]["TXInterval"] = String(actconf.tinterval);
  json_Device["Device"]["LoRaSettings"]["TimeSlot"] = String(slot);
  json_Device["Device"]["LoRaSettings"]["TXCounter"] = String(LMIC.seqnoUp - 1);
  json_Device["Device"]["LoRaSettings"]["Relay"] = String(actconf.relay);

  json_Device["Device"]["DisplaySettings"]["Skin"] = String(actconf.skin);
  json_Device["Device"]["DisplaySettings"]["InstrumentSize"] = String(actconf.instrumentSize);

  json_Device["Device"]["DeviceSettings"]["SerialDebugMode"] = String(actconf.debug);
  json_Device["Device"]["DeviceSettings"]["SerialSpeed"] = String(actconf.serspeed);
  json_Device["Device"]["DeviceSettings"]["DeviceID"] = String(actconf.deviceID);
  json_Device["Device"]["DeviceSettings"]["DeviceType"] = String(actconf.deviceType);
  json_Device["Device"]["DeviceSettings"]["SendData"] = String(actconf.senddata);
  json_Device["Device"]["DeviceSettings"]["VoltageOffset"] = String(actconf.voffset);
  json_Device["Device"]["DeviceSettings"]["VoltageSlopeA1"] = String(actconf.a1vslope);
  json_Device["Device"]["DeviceSettings"]["VoltageSlopeA2"] = String(actconf.a2vslope);
  json_Device["Device"]["DeviceSettings"]["VoltageAverage"] = String(actconf.vaverage);
  json_Device["Device"]["DeviceSettings"]["Tank1Offset"] = String(actconf.t1offset);
  json_Device["Device"]["DeviceSettings"]["Tank1SlopeA1"] = String(actconf.a1t1slope);
  json_Device["Device"]["DeviceSettings"]["Tank1SlopeA2"] = String(actconf.a2t1slope);
  json_Device["Device"]["DeviceSettings"]["Tank1Average"] = String(actconf.t1average);
  json_Device["Device"]["DeviceSettings"]["Tank2Offset"] = String(actconf.t2offset);
  json_Device["Device"]["DeviceSettings"]["Tank2SlopeA1"] = String(actconf.a1t2slope);
  json_Device["Device"]["DeviceSettings"]["Tank2SlopeA2"] = String(actconf.a2t2slope);
  json_Device["Device"]["DeviceSettings"]["Tank2Average"] = String(actconf.t2average);
  json_Device["Device"]["DeviceSettings"]["TempSensorType"] = String(actconf.tempSensorType);
  json_Device["Device"]["DeviceSettings"]["TempUnit"] = String(actconf.tempUnit);
  json_Device["Device"]["DeviceSettings"]["envSensor"] = String(actconf.envSensor);
  json_Device["Device"]["DeviceSettings"]["standbyMode"] = String(actconf.standbyMode);

  json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Value"] = String(temperature, 1);
  json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Unit"] = String(actconf.tempUnit);

  json_Device["Device"]["AirPressure"]["AirTemperature"]["Value"] = String(pressure, 0);
  json_Device["Device"]["AirPressure"]["AirTemperature"]["Unit"] = "mbar";

  json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Value"] = String(humidity, 0);
  json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Unit"] = "%";

  json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Value"] = String(dewp, 1);
  json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Unit"] = String(actconf.tempUnit);

  json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Value"] = String(temp1wire, 1);
  json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Unit"] = String(actconf.tempUnit);

  json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Value"] = String(voltage, 3);
  json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Unit"] = "V";

  json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Value"] = String(capacity, 0);
  json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Unit"] = "%";

  json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Value"] = String(tank1, 3);
  json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Unit"] = "V";

  json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Value"] = String(tank2, 3);
  json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Unit"] = "V";

  json_Device["Device"]["MeasuringValues"]["Tank1"]["Value"] = String(tank1p, 3);
  json_Device["Device"]["MeasuringValues"]["Tank1"]["Unit"] = "%";

  json_Device["Device"]["MeasuringValues"]["Tank2"]["Value"] = String(tank2p, 3);
  json_Device["Device"]["MeasuringValues"]["Tank2"]["Unit"] = "%";

  json_Device["Device"]["MeasuringValues"]["Alarm"]["Value"] = String(alarm1);
  json_Device["Device"]["MeasuringValues"]["Alarm"]["Unit"] = "bin";

  json_Device["Device"]["MeasuringValues"]["Relay"]["Value"] = String(actconf.relay);
  json_Device["Device"]["MeasuringValues"]["Relay"]["Unit"] = "bin";

  json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Value"] = String(int(relaytimer * 5));
  json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Unit"] = "x5min";

  json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Value"] = String(actconf.envSensor);
  json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Unit"] = "";

  json_Device["Device"]["MeasuringValues"]["standbyMode"]["Value"] = String(actconf.standbyMode);
  json_Device["Device"]["MeasuringValues"]["standbyMode"]["Unit"] = "";

  json_Device["Device"]["MeasuringValues"]["loraStandbyMode"]["Value"] = String(actconf.loraStandbyMode);
  json_Device["Device"]["MeasuringValues"]["loraStandbyMode"]["Unit"] = "";

  json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Value"] = "%vedirectVoltage%";
  json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Unit"] = "V";

  json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Value"] = "%vedirectCurrent%";
  json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Unit"] = "A";

  json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Value"] = "%vedirectTemp%";
  json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Unit"] = "°C";

  json_Device["Device"]["MeasuringValues"]["Latitude"]["Value"] = String(latitude, 6);
  json_Device["Device"]["MeasuringValues"]["Latitude"]["Unit"] = String(latitudeNS);

  json_Device["Device"]["MeasuringValues"]["Longitude"]["Value"] = String(longitude, 6);
  json_Device["Device"]["MeasuringValues"]["Longitude"]["Unit"] = String(longitudeEW);

  json_Device["Device"]["MeasuringValues"]["Altitude"]["Value"] = String(altitude, 0);
  json_Device["Device"]["MeasuringValues"]["Altitude"]["Unit"] = "m";

  String zhour = firstzero(hour);
  String zminute = firstzero(minute);
  String zsecond = firstzero(second); 
  String timestr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
  json_Device["Device"]["MeasuringValues"]["Time"]["Value"] = String(timestr);
  json_Device["Device"]["MeasuringValues"]["Time"]["Unit"] = "UTC";

  String zday = firstzero(day);
  String zmonth = firstzero(month);
  String zyear = firstzero(year);
  String datestr = "\"" + String(zday) + "." + String(zmonth) + "." + String(zyear) + "\"";
  json_Device["Device"]["MeasuringValues"]["Date"]["Value"] = String(datestr);
  json_Device["Device"]["MeasuringValues"]["Date"]["Unit"] = "GTM";

  String sunrisestr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
  json_Device["Device"]["MeasuringValues"]["Sunrise"]["Value"] = String(sunrisestr);
  json_Device["Device"]["MeasuringValues"]["Sunrise"]["Unit"] = "UTC";

  String sunsetstr = "\"" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "\"";
  json_Device["Device"]["MeasuringValues"]["Sunset"]["Value"] = String(sunsetstr);
  json_Device["Device"]["MeasuringValues"]["Sunset"]["Unit"] = "UTC";

  json_Device["Device"]["MeasuringValues"]["Speed"]["Value"] = String(gpsspeed);
  json_Device["Device"]["MeasuringValues"]["Speed"]["Unit"] = "kn";

  json_Device["Device"]["MeasuringValues"]["Course"]["Value"] = String(course);
  json_Device["Device"]["MeasuringValues"]["Course"]["Unit"] = "°";

  json_Device["Device"]["NMEAValues"]["String1"] = sendXDR1(0);
  json_Device["Device"]["NMEAValues"]["String2"] = sendXDR2(0);
  json_Device["Device"]["NMEAValues"]["String3"] = sendXDR3(0);
  json_Device["Device"]["NMEAValues"]["String4"] = "°";
  json_Device["Device"]["NMEAValues"]["String5"] = "°";

  String stringjsondata = "";
  serializeJson(json_Device, stringjsondata);

  //httpServer.sendHeader("Access-Control-Allow-Origin", "*"); // Needs new browser for CORS (Cross Origin Resource Sharing)
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "application/json", stringjsondata);
  request->send(200, "application/json", stringjsondata);

  //elapsedMillis = millis() - previousMillis;
  //Serial.print("Wert Stoppuhr: ");
  //Serial.println(elapsedMillis);
});

httpServer.on("/json_old", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/json_old", []() {
  unsigned long previousMillis = 0;
  unsigned long elapsedMillis = 0;
  previousMillis = millis();
  String content = readFile2(LittleFS, "/json.json");

  content.replace("%devname%", String(actconf.devname));
  content.replace("%lorafrequency%", String(actconf.lorafrequency));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  content.replace("%wlanquality%", String(wlanquality()));
  content.replace("%license%", String(actconf.license));
  content.replace("%getSdkVersion%", String(ESP.getSdkVersion()) );
  content.replace("%chipId%", String(chipId));
  content.replace("%getCpuFreqMHz%", String(ESP.getCpuFreqMHz()));
  content.replace("%getFreeHeap%", String(ESP.getFreeHeap()));
  content.replace("%hname%", String(hname));
  String mdnsname = "";
    if(actconf.mDNS == 1){
      mdnsname = String(hname) + ".local";
    }
    else{
      mdnsname =F( "not activ");
    }
  content.replace("%mdnsname%", mdnsname);
  content.replace("%serverMode%", String(actconf.serverMode));
  content.replace("%sssid%", String(actconf.sssid));
  content.replace("%softAPIP%", WiFi.softAPIP().toString());
  //content.replace("%WiFichannel%", String(WiFi.channel()));
  //content.replace("%WiFichannel%", String(actconf.cssid));
  content.replace("%cssid%", WiFi.localIP().toString());
  content.replace("%fieldstrength%", String(fieldstrength));
  content.replace("%quality%", String(quality));
  String mystring = String(actconf.devaddr, HEX);
  mystring.toUpperCase();
  content.replace("%devaddr%", mystring);
  content.replace("%txChnl%", String(LMIC.txChnl));
  content.replace("%sf%", String(sf));
  content.replace("%slot%", String(slot));
  content.replace("%seqnoUp%", String(LMIC.seqnoUp - 1));

  content.replace("%latitude%", String(latitude, 6));
  content.replace("%latitudeNS%", String(latitudeNS));
  content.replace("%longitude%", String(longitude, 6));
  content.replace("%longitudeEW%", String(longitudeEW));
  content.replace("%gpsspeed%", String(gpsspeed));
  content.replace("%course%", String(course));
  content.replace("%altitude%", String(altitude, 0));
  content.replace("%voltage%", String(voltage, 3));
  content.replace("%capacity%", String(capacity, 0));
  content.replace("%temp1wire%", String(temp1wire, 1));
  content.replace("%tempUnit%", String(actconf.tempUnit));
  content.replace("%tank1%", String(tank1, 3));
  content.replace("%tank1p%", String(tank1p, 0));
  content.replace("%tank2%", String(tank2, 3));
  content.replace("%tank2p%", String(tank2p, 0));
  content.replace("%alarm1%", String(alarm1));
  content.replace("%relay%", String(actconf.relay));
  content.replace("%relaytimer%", String(int(relaytimer * 5)));
  content.replace("%envSensor%", String(actconf.envSensor));
  content.replace("%standbyMode%", String(actconf.standbyMode));
  content.replace("%loraStandbyMode%", String(actconf.loraStandbyMode));

  String zhour = firstzero(hour);
    String zminute = firstzero(minute);
    String zsecond = firstzero(second);
    String timestr = "" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "";
    content.replace("%timestr%", timestr);

    String zday = firstzero(day);
    String zmonth = firstzero(month);
    String zyear = firstzero(year);
    String datestr = "" + String(zday) + "." + String(zmonth) + "." + String(zyear) + "";
    content.replace("%datestr%", datestr);

    String sunrisestr = "" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "";
    content.replace("%sunrisestr%", sunrisestr);

    String sunsetstr = "" + String(zhour) + ":" + String(zminute) + ":" + String(zsecond) + "";
    content.replace("%sunsetstr%", sunsetstr);

  //httpServer.sendHeader("Access-Control-Allow-Origin", "*"); // Needs new browser for CORS (Cross Origin Resource Sharing)
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "application/json", content);
  request->send(200, "application/json", content);

  elapsedMillis = millis() - previousMillis;
  Serial.print("Wert Stoppuhr: ");
  Serial.println(elapsedMillis);
});

// Use no cash because the js was permanently modifyed (transaction ID)
httpServer.on("/MD5", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/MD5", []() {
  String content = readFile2(LittleFS, "/md5.js");
  content.replace("%transactionID%", String(transactionID));
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/javascript", content);
  request->send(200, "text/javascript", content);
});

// Use no cash because the js was permanently modifyed (transaction ID)
//httpServer.on("/filesystem2", HTTP_POST, [](AsyncWebServerRequest *request) {
  httpServer.on("/filesystem", HTTP_GET, [](AsyncWebServerRequest *request) {
  Serial.print("http request");
//httpServer.on("/filesystem", []() {
  String content = handleRoot(LittleFS, "/", 0);
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", content);
  request->send(200, "text/html", content);
});

// Firmware update
/*httpServer.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
//httpServer.on("/update", HTTP_POST, []() {
  //httpServer.sendHeader("Connection", "close");
  //httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK"));
  ESP.restart();
}, []() {
// Update routine
 HTTPUpload& upload = httpServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
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
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } 
});*/

// run handleUpload function when any file is uploaded
  /*server->on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      }, handleUpload);*/

  /*handling uploading file */
  /*httpServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  //httpServer.on("/upload", HTTP_POST, [](){
    httpServer.sendHeader("Connection", "close");
  },[](){  
    HTTPUpload& upload = httpServer.upload();
    if(opened == false){
      opened = true;
      root = LittleFS.open((String("/") + upload.filename).c_str(), FILE_WRITE);
      if(!root){
        Serial.println("- failed to open file for writing");
        return;
      }
    } 
    if(upload.status == UPLOAD_FILE_WRITE){
      if(root.write(upload.buf, upload.currentSize) != upload.currentSize){
        Serial.println("- failed to write");
        return;
      }
    } else if(upload.status == UPLOAD_FILE_END){
      root.close();
      Serial.println("UPLOAD_FILE_END");
      opened = false;
    }
  });*/

httpServer.on("/formatfs", HTTP_POST, [](AsyncWebServerRequest *request) {
//httpServer.on("/formatfs", []() {
  formatfs(LittleFS);
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", "done");
  request->send(200, "text/html", "done");
});

httpServer.on("/updatefiles", HTTP_POST, [](AsyncWebServerRequest *request) {
//httpServer.on("/updatefiles", []() {
  //DownloadFilesFromFtp(actconf.fversion);
  //DownloadFilesFromWeb(actconf.fversion);
  runDownloadingFiles = true;
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", "done");
  request->send(200, "text/html", "done");
});

httpServer.on("/updatefilesstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
//httpServer.on("/updatefiles", []() {
  //DownloadFilesFromFtp(actconf.fversion);
  //DownloadFilesFromWeb(actconf.fversion);
  String test = (String)runDownloadingFiles;
  //httpServer.sendHeader("Cache-Control", "no-cache");
  //httpServer.send(200, "text/html", "done");
  request->send(200, "text/html", test);
});

httpServer.onNotFound([](AsyncWebServerRequest *request){
  String content = readFile2(LittleFS, "/error.html");
  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  content.replace("%wlanquality%", String(wlanquality()));
  //httpServer.send(404, "text/html", content);
  request->send(404, "text/html", content);
  //request->send(404, "text/plain", "The content you are looking for was not found.");
});
