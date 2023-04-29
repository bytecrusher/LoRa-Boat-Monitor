#include "func_webclient.h"
#include <WiFi.h>       // WiFi lib with TCP server and client
#include <WiFiClient.h> // WiFi lib for clients

#include <ESP32_FTPClient.h>
#include "FS.h"
#include <LittleFS.h>

void DownloadFilesFromWeb(char *fversion)
{
    testDownload("css_black.css", fversion);
    testDownload("css_red.css", fversion);
    testDownload("css_white.css", fversion);
    testDownload("devinfo.html", fversion);
    testDownload("error.html", fversion);
    testDownload("favicon.ico", fversion);
    testDownload("firmware.html", fversion);
    testDownload("app.js", fversion);
    //testDownload("json.json", fversion);
    testDownload("lora.html", fversion);
    testDownload("main.html", fversion);
    testDownload("md5.js", fversion);
    testDownload("password.html", fversion);
    testDownload("restart.html", fversion);
    testDownload("sensorv.html", fversion);
    testDownload("settings.html", fversion);
}

void testDownload(const char *fileName, char *fversion)
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
