#include "func_ftpclient.h"
#include <WiFi.h> // WiFi lib with TCP server and client

#include <ESP32_FTPClient.h>
#include "FS.h"
#include <LittleFS.h>

#include "func_myFunctions.h"

char ftp_server[] = "192.168.178.136";
char ftp_user[] = "ftptest";
char ftp_pass[] = "zydxeM-vyztop-5bijny";

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp(ftp_server, ftp_user, ftp_pass, 5000, 2);

void DownloadFilesFromFtp(char *fversion)
{
  openFtpConnection(fversion);
  // listFTPdir();
  getFileFromFtp("css_black.css");
  getFileFromFtp("css_red.css");
  getFileFromFtp("css_white.css");
  getFileFromFtp("devinfo.html");
  getFileFromFtp("error.html");
  getFileFromFtp("favicon.ico");
  getFileFromFtp("firmware.html");
  getFileFromFtp("gauge.min.js");
  getFileFromFtp("header.html");
  getFileFromFtp("app.js");
  getFileFromFtp("lora.html");
  getFileFromFtp("index.html");
  getFileFromFtp("md5.js");
  getFileFromFtp("password.html");
  getFileFromFtp("restart.html");
  getFileFromFtp("sensorv.html");
  getFileFromFtp("settings.html");
  getFileFromFtp("settings.js");
  closeFtpConnection();
}

void openFtpConnection(char *fversion)
{
  ftp.OpenConnection();
  ftp.InitFile("Type A");
  ftp.ChangeWorkDir("/V1.0x/");
}

void closeFtpConnection()
{
  ftp.CloseConnection();
}

void listFTPdir()
{
  // Get directory content
  ftp.InitFile("Type A");
  ftp.ChangeWorkDir("/V1.0x/");

  // Download the text file or read it
  String response = "";
  // Get the file size
  size_t fileSize = 0;
  String list[128];

  ftp.InitFile("Type A");
  ftp.ContentList("", list);
  for (uint8_t i = 0; i < sizeof(list); i++)
  {
    uint8_t indexSize = 0;
    uint8_t indexMod = 0;
    if (list[i].length() > 0)
    {
      list[i].toLowerCase();
      // Print the directory details
      DebugPrintln(3, String(list[i]));
    }
    else
      break;
  }
}

void getFileFromFtp(const char *fileName)
{
  File root2;
  bool opened2 = false;

  // Download the text file or read it
  String response = "";
  // Get the file size
  size_t fileSize = 0;
  String list[128];

  ftp.InitFile("Type A");
  ftp.ContentList("", list);
  for (uint8_t i = 0; i < sizeof(list); i++)
  {
    uint8_t indexSize = 0;
    uint8_t indexMod = 0;
    if (list[i].length() > 0)
    {
      list[i].toLowerCase();
      if (list[i].indexOf(fileName) > -1)
      {
        indexSize = list[i].indexOf("size") + 5;
        indexMod = list[i].indexOf("modify") - 1;
        fileSize = list[i].substring(indexSize, indexMod).toInt();
      }
    }
    else
      break;
  }

  // Dynammically alocate buffer
  unsigned char *downloaded_file = (unsigned char *)malloc(fileSize);
  ftp.InitFile("Type I");
  ftp.DownloadFile(fileName, downloaded_file, fileSize, false);
  DebugPrintln(3, "The file is downloaded.");
  //DebugPrint(3, "fileSize: ");
  //DebugPrintln(3, String(fileSize));

  if (opened2 == false)
  {
    opened2 = true;
    root2 = LittleFS.open((String("/") + String(fileName)).c_str(), FILE_WRITE);
    if (!root2)
    {
      DebugPrintln(1, "- failed to open file for writing");
      return;
    }
  }
  if (root2.write(downloaded_file, fileSize) != fileSize)
  {
    DebugPrintln(1, "- failed to write");
    return;
  }
  root2.close();
  free(downloaded_file);
}
