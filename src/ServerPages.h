// Server pages
// Insert this library after server definition

httpServer.on("/old", []() {
  // Read all received get arguments and save in a array
  int num = httpServer.args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    vname[i] = httpServer.argName(i);
    value[i] = httpServer.arg(i);
    // Check the return value from Restart web page
    if(vname[i] == "restart" &&  value[i] == "1"){
      resetESP = 1;
    }
    else{
      resetESP = 0;
    }
  } 
  // Send page     
  //String content = Startpage(num, vname, value);
  String content = "";
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);

  // Restart routine
  if(resetESP == 1){
    delay(3000); // Waiting time for system restart
    resetESP = 0;
    // Restart the ESP32
    ESP.restart();
  }      
});

//httpServer.serveStatic("/", LittleFS, "/main.html");

httpServer.on("/", []() {
  String content = readFile2(LittleFS, "/main.html");
  if (content == "- failed to open file for reading"){
    content += handleRoot(LittleFS, "/", 0);
  }
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});

httpServer.on("/test.html", []() {
  Serial.print(ESP.getFreeHeap());
  Serial.print(ESP.getMaxAllocHeap());
  String content = readFile2(LittleFS, "/test.html");
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
  Serial.print(ESP.getFreeHeap());
  Serial.print(ESP.getMaxAllocHeap());
});

httpServer.on("/sensorv", []() {
  // Send page
  String content = Sensorv();
  httpServer.sendHeader("Cache-Control", "max-age=600");
  httpServer.send(200, "text/html", content);  
});

httpServer.on("/lora", []() {
  // Send page
  String content = Lora();
  httpServer.sendHeader("Cache-Control", "max-age=600");
  httpServer.send(200, "text/html", content);  
});

httpServer.on("/settings", []() {
  // Read all received get arguments and save in a array
  int num = httpServer.args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    vname[i] = httpServer.argName(i);
    value[i] = httpServer.arg(i);  
  } 
  // Send page
  String content = Settings(num, vname, value);
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);  
});

httpServer.on("/restart", []() {
  // Read all received get arguments and save in a array
  int num = httpServer.args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    vname[i] = httpServer.argName(i);
    value[i] = httpServer.arg(i);  
  } 
  // Send page
  String content = Reset(num, vname, value);
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});

httpServer.on("/firmware", []() {
  // Read all received get arguments and save in a array
  int num = httpServer.args();
  String vname[num];
  String value[num];
  for (int i = 0; i < num; i++) {
    vname[i] = httpServer.argName(i);
    value[i] = httpServer.arg(i);  
  } 
  // Send page
  String content = Firmware(num, vname, value);
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});

httpServer.on("/devinfo", []() {
  //String content = Devinfo();
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
  
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});

/*httpServer.on("/devinfo2", []() {
  //String content = Devinfo();
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});*/

httpServer.on("/favicon.ico", []() {
  //String content = Icon();
  String content = readFile2(LittleFS, "/favicon.ico");
  httpServer.sendHeader("Cache-Control", "max-age=600");
  httpServer.send(200, "image/svg+xml", content);
});

httpServer.on("/css", []() {
  String content = CSS();
   httpServer.sendHeader("Cache-Control", "max-age=1");
  httpServer.send(200, "text/css", content);
});

httpServer.on("/js", []() {
  //String content = JS();
  String content = readFile2(LittleFS, "/js.js");
  httpServer.sendHeader("Cache-Control", "max-age=600");
  httpServer.send(200, "text/javascript", content);
});

httpServer.on("/json", []() {
  String content = JSON();
  httpServer.sendHeader("Access-Control-Allow-Origin", "*"); // Needs new browser for CORS (Cross Origin Resource Sharing)
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "application/json", content);
});

// Use no cash because the js was permanently modifyed (transaction ID)
httpServer.on("/MD5.js", []() {
  String content = MD5();
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/javascript", content);
});

// Firmware update
httpServer.on("/update", HTTP_POST, []() {
  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
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
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } 
});

  /*handling uploading file */
  httpServer.on("/upload", HTTP_POST, [](){
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
  });

httpServer.onNotFound([]() {
  //String content = Error();
  String content = readFile2(LittleFS, "/error.html");
  content.replace("%devname%", String(actconf.devname));
  content.replace("%crights%", String(actconf.crights));
  content.replace("%fversion%", String(actconf.fversion));
  content.replace("%wlanquality%", String(wlanquality()));
  httpServer.send(404, "text/html", content);
});

// Use no cash because the js was permanently modifyed (transaction ID)
httpServer.on("/filesystem", []() {
  String content = handleRoot(LittleFS, "/", 0);
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", content);
});

httpServer.on("/formatfs", []() {
  formatfs(LittleFS);
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", "done");
});

httpServer.on("/updatefiles", []() {
  //updatefiles(LittleFS);
  DownloadFilesFromFtp();
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.send(200, "text/html", "done");
});
