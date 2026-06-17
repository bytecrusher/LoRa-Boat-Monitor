#ifndef _func_WebServerHandler_H
#define _func_WebServerHandler_H

#include <Arduino.h>            // Arduino Environment
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>  // asynchron webserver lib
#include <LittleFS.h>
#include "initialsetup_html.h"  // HTML file for initial setup of the devide (if filesystem is formated)
#include "func_myFunctions.h"

extern String readFile2(fs::FS &fs, const char * path);
extern String getheader(configData actconf);
extern configData actconf;
extern boolean runDownloadingFiles;
extern boolean runDownloadingFilesStatus;
extern size_t webFilesDownloadCompleted;
extern size_t webFilesDownloadTotal;
extern String webFilesDownloadCurrentName;
extern String webFilesDownloadStatusMessage;
extern bool webFilesDownloadError;
extern uint8_t webFilesDownloadRetryCount;
extern unsigned long webFilesDownloadStartedMillis;
extern bool remoteOtaPending;
extern bool remoteOtaInProgress;
extern String remoteOtaUrl;
extern String remoteOtaVersion;
extern String remoteOtaSha256;
extern bool remoteOtaUseMdsEndpoint;
extern bool remoteOtaRebootRequired;
extern String formatfs(fs::FS &fs);
extern configData defconf;
extern int slot;
extern String getMyDirAsString(fs::FS &fs, const char * dirname, uint8_t levels);
extern bool reboot;
extern unsigned long standbySleepBlockedUntilMillis;
extern bool localOtaInProgress;
extern unsigned long scheduledRestartMillis;
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
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage);
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256);
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256, bool useMdsOtaEndpoint);
String getCsrfToken();
bool isCsrfTokenValid(AsyncWebServerRequest *request);
bool requireCsrfToken(AsyncWebServerRequest *request);
String maskSecret(const String &secret);
void printProgress(size_t prg, size_t sz);

#endif
