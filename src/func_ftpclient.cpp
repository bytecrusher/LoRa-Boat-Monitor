#include "func_ftpclient.h"
#include <WiFi.h>               // WiFi lib with TCP server and client
#include <WiFiClient.h>         // WiFi lib for clients

#include <ESP32_FTPClient.h>
#include "FS.h"
#include <LittleFS.h>

char ftp_server[] = "192.168.178.136";
char ftp_user[]   = "ftptest";
char ftp_pass[]   = "zydxeM-vyztop-5bijny";

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 2);

void DownloadFilesFromFtp() {
  //listFTPdir();
  //getFileFromFtp("css.css");
  getFileFromFtp("devinfo.html");
  getFileFromFtp("error.html");
  getFileFromFtp("favicon.ico");
  //getFileFromFtp("firmware.html");
  getFileFromFtp("js.js");
  //getFileFromFtp("json.json");
  //getFileFromFtp("lora.html");
  getFileFromFtp("main.html");
  //getFileFromFtp("MD5.html");
  //getFileFromFtp("restart.html");
  //getFileFromFtp("sensorv.html");
  //getFileFromFtp("settings.html");
}

void listFTPdir() {
  ftp.OpenConnection();

  // Get directory content
  ftp.InitFile("Type A");
  ftp.ChangeWorkDir("/home/");

  //Download the text file or read it
  String response = "";
  // Get the file size
  size_t       fileSize = 0;
  String       list[128];

  ftp.InitFile("Type A");
  ftp.ContentList("", list);
  for( uint8_t i = 0; i < sizeof(list); i++)
  {
    uint8_t indexSize = 0;
    uint8_t indexMod  = 0;
    if(list[i].length() > 0)
    {
      list[i].toLowerCase();
      /*if( list[i].indexOf(fileName) > -1 )
      {
        indexSize = list[i].indexOf("size") + 5;
        indexMod  = list[i].indexOf("modify") - 1;
        fileSize = list[i].substring(indexSize, indexMod).toInt();
      }*/

      // Print the directory details
      Serial.println(list[i]);
    }
    else
      break;
  }
ftp.CloseConnection();
}

void getFileFromFtp(const char * fileName) {
//--------------------------------
  File root2;
  bool opened2 = false;
  ftp.OpenConnection();

  // Get directory content
  ftp.InitFile("Type A");
  ftp.ChangeWorkDir("/home/");

  //Download the text file or read it
  String response = "";
  // Get the file size
  size_t       fileSize = 0;
  String       list[128];

  ftp.InitFile("Type A");
  ftp.ContentList("", list);
  for( uint8_t i = 0; i < sizeof(list); i++)
  {
    uint8_t indexSize = 0;
    uint8_t indexMod  = 0;
    if(list[i].length() > 0)
    {
      list[i].toLowerCase();
      if( list[i].indexOf(fileName) > -1 )
      {
        indexSize = list[i].indexOf("size") + 5;
        indexMod  = list[i].indexOf("modify") - 1;
        fileSize = list[i].substring(indexSize, indexMod).toInt();
      }

      // Print the directory details
      //Serial.println(list[i]);
    }
    else
      break;
  }

  //Dynammically alocate buffer
  unsigned char * downloaded_file = (unsigned char *) malloc(fileSize);
  ftp.InitFile("Type I");
  ftp.DownloadFile(fileName, downloaded_file, fileSize, false);
  ftp.InitFile("Type I");
  ftp.DownloadString(fileName, response);
  //Serial.println("The file content is: " + response);
  Serial.println("The file is downloaded.");
  //Serial.print("fileSize: ");
  //Serial.println(fileSize);
  
  if(opened2 == false){
      opened2 = true;
      root2 = LittleFS.open((String("/") + String(fileName) ).c_str(), FILE_WRITE);
      if(!root2){
        Serial.println("- failed to open file for writing");
        return;
      }
    }
    //root2.write(response);
    //if(upload.status == UPLOAD_FILE_WRITE){
      if(root2.write(downloaded_file, fileSize) != fileSize){
        Serial.println("- failed to write");
        return;
      }
      root2.close();
    //} else if(upload.status == UPLOAD_FILE_END){
      //root2.close();
      //Serial.println("UPLOAD_FILE_END");
      //opened2 = false;
    //}

  /*String list[128];
  //ftp.ChangeWorkDir("/home/");
  ftp.InitFile("Type A");
  ftp.ContentList("", list);
  Serial.println("\nDirectory info: ");
  for(int i = 0; i < sizeof(list); i++)
  {
    if(list[i].length() > 0)
      Serial.println(list[i]);
    else
      break;
  }*/

  ftp.CloseConnection();
  //--------------------------------
}
