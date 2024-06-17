#ifndef _func_WebServerHandler_H
#define _func_WebServerHandler_H

#include <Arduino.h>            // Arduino Environment
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>  // asynchron webserver lib
#include <LittleFS.h>
#include "initialsetup_html.h"  // HTML file for initial setup of the devide (if filesystem is formated)
#include "wificonfig_html.h"    // HTML file for wifi config. (obsolete)
#include "func_myFunctions.h"

extern String readFile2(fs::FS &fs, const char * path);
extern String getheader(configData actconf);
extern configData actconf;
extern int resetESP;
extern boolean runDownloadingFiles;
extern String formatfs(fs::FS &fs);
extern configData defconf;
extern int slot;
extern String getMyDirAsString(fs::FS &fs, const char * dirname, uint8_t levels);
extern bool reboot;
extern int relayPin;
extern String SendDataViaWifi[2];
extern String usepassword[2];
extern String isize[9];
extern String timeout[11];
extern String apchannel[13];
extern String servermode[5];
extern String mdnsservice[2];
extern String lorafrequencys[2];
extern String lchannel[10];
extern String spreadf[4];
extern String dynsf[2];
extern String relay[3];
extern String debugmode[4];
extern String serspeed[10];
extern String WebSerialDebug[2];
extern String deviceid[10];
extern String senddata[2];
extern String vaverage[10];
extern String t1average[10];
extern String t2average[10];
extern String tstype[2];
extern String tempunits[2];
extern String envSensor[4];
extern String standbyMode[2];
extern String loraOperationMode[4];
extern String WifiStandbyMode[2];
extern String cssStyle[3];
extern String OledDisplayRotation[2];
extern uint chipId;
extern String hname;
extern AsyncWebServer httpServer;
extern uint8_t getLMICtxChnl();
extern uint32_t getLMICseqnoUp();

void WebServerHandler();
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
void handleDoUpdateBeta(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
void printProgress(size_t prg, size_t sz);

#endif