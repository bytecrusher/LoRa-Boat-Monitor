#include "func_webclient.h"
#include <WiFi.h>       // WiFi lib with TCP server and client
#include <WiFiClient.h> // WiFi lib for clients
#include <WiFiClientSecure.h>

#include <ESP32_FTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Configuration.h>

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
    WiFiClient client2;
    File root2;
    bool opened2 = false;
    size_t fileSize = 0;

    const uint16_t port = 80;
    const char *host = "loraboatmonitorwebserverdata.derguntmar.de";

    Serial.print("Connecting to website: ");
    Serial.println(host);

    String line = "";
    String header = "";
    if (client2.connect(host, port))
    {
        //githubpath = "data/app.js"
        //https://raw.githubusercontent.com/bytecrusher/LoRa-Boat-Monitor/main/data/lora.html
        
        String url = "/files_for_esp_webserver/" + (String)fversion + "/" + (String)fileName;
        client2.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "User-Agent: ESP32\r\n" + "Connection: close\r\n\r\n");
//vTaskDelay(10);
        for (int i = 0; i <= 10; i++)
        {
            String headertemp = client2.readStringUntil('\n');
            header += headertemp + "\n";
            if (header == "\r")
            {
                break;
            }
        }
//        vTaskDelay(10);
        line = client2.readString();
        //  ToDo: Check if header contains not 404 !!!
        Serial.print("data länge: ");
        Serial.println(line.length());
        Serial.println("The file is downloaded.");
        // Serial.print("fileSize: ");
        // Serial.println(fileSize);
        client2.stop();

        if (opened2 == false)
        {
            opened2 = true;
            root2 = LittleFS.open((String("/") + String(fileName)).c_str(), FILE_WRITE);
            if (!root2)
            {
                Serial.println("- failed to open file for writing");
                return;
            }
        }
        root2.print(line);
        /*if(root2.write(line, linelength) != linelength){
          Serial.println("- failed to write");
          return;
        }*/
        root2.close();
    }
    else
    {
        Serial.println("Connection unsucessful");
    }
    Serial.print("end.");
}

void sendToMDS(configData actconf)
{
    WiFiClientSecure client3;
    #define protocollversion "1"
    String MacAddress = WiFi.macAddress();
    StaticJsonDocument<600> allSensors;

    /*HTTPClient http;
      // Your Domain name with URL path or IP address with path
      http.begin(actconf.MdsUrl);
      // Specify content-type header
      http.addHeader("Content-Type", "application/json");*/

      // Loop through each device, print out temperature data
        //for (uint8_t i = 0; i < numberOfDevices; i++)
        //{
            //allSensors[i] = collectJsonData();
        //}

        //allSensors[0] = collectJsonData();

        StaticJsonDocument<200> sensorData;
        String buffer = "srgrdfg";
        String tempC = "20";
        String buffDate = "28.07.2023";
        String buffTime = "12.55";
        //sensorData["typid"] = 1;
        sensorData["sensorId"] = 39;
        //sensorData["sensorAddress"] = buffer;
        sensorData["sensorAddress"] = 1;
        sensorData["value1"] = String(tempC);
        sensorData["date"] = String(buffDate);
        sensorData["time"] = String(buffTime);

        allSensors[0] = sensorData;
        allSensors[1] = sensorData;

        StaticJsonDocument<200> board;
      StaticJsonDocument<1024> docpayload;
      // Add values in the document
      board["api_key"] = actconf.MdsApiKey;
      board["protocollversion"] = protocollversion;
      board["macaddress"] = MacAddress;
      docpayload["board"] = board;
      docpayload["sensors"] = allSensors;
        
      String requestBody;
      serializeJson(docpayload, requestBody);

    //Serial.print("requestBody: ");
    //Serial.println(requestBody);
        
      /*int httpResponseCode = http.POST(requestBody);
      Serial.print("httppost via json: ");
      Serial.println(requestBody);
      Serial.print("httpResponseCode: ");
      Serial.println(httpResponseCode);
      if(httpResponseCode > 0){
        String response = http.getString();
        Serial.println(httpResponseCode);
        Serial.println(response);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();*/

    client3.setInsecure();
    //client3.connect(actconf.MdsUrl, 443);

    Serial.println("\nStarting connection to server...");
  //if (!client3.connect(actconf.MdsUrl, 443))
  if (!client3.connect("mds-git.derguntmar.de", 443))
    Serial.println("Connection failed!");
  else {
    Serial.println("Connected to server!");
    // Make a HTTP request:
    //client3.println("GET https://mds-git.derguntmar.de/receiver/receivejson.php HTTP/1.0");
    //client3.println("Host: mds-git.derguntmar.de");
    //client3.println("Connection: close");
    //client3.println();

    client3.println("POST  https://mds-git.derguntmar.de/receiver/receivejson.php HTTP/1.1");
    client3.println("Host: mds-git.derguntmar.de");
    client3.println("content-length:"+ String(requestBody.length()));
    client3.println("Connection: close");
    client3.println();
    client3.println(requestBody);

    while (client3.connected()) {
      String line = client3.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client3.available()) {
      char c = client3.read();
      Serial.write(c);
    }

    client3.stop();
  }
}

/*StaticJsonDocument<200> collectJsonData()
{
  StaticJsonDocument<200> sensorData;
  String buffer = "srgrdfg";
  String tempC = "20";
  String buffDate = "28.07.2023";
  String buffTime = "12.55";
  sensorData["typid"] = 1;
  sensorData["sensorAddress"] = buffer;
  sensorData["value1"] = String(tempC);
  sensorData["date"] = String(buffDate);
  sensorData["time"] = String(buffTime);

  return sensorData;
}*/