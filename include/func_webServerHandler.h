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
extern volatile boolean runDownloadingFiles;
extern volatile boolean runDownloadingFilesStatus;
extern String formatfs(fs::FS &fs);
extern configData defconf;
extern int slot;
extern String getMyDirAsString(fs::FS &fs, const char * dirname, uint8_t levels);
extern bool reboot;
extern unsigned long standbySleepBlockedUntilMillis;
extern bool localOtaInProgress;
extern unsigned long scheduledRestartMillis;
extern bool keepAwakeAfterUpdateRestart;
extern int relayPin;
extern String SendDataViaWifi[3];
extern String usepassword[3];
extern String isize[10];
extern String timeout[12];
extern String apchannel[14];
extern String servermode[6];
extern String mdnsservice[3];
extern String lorafrequencys[3];
extern String lchannel[11];
extern String spreadf[5];
extern String dynsf[3];
extern String relay[4];
extern String debugmode[5];
extern String serspeed[11];
extern String WebSerialDebug[3];
extern String deviceid[11];
extern String senddata[3];
extern String vaverage[11];
extern String t1average[11];
extern String t2average[11];
extern String tstype[3];
extern String tempunits[3];
extern String envSensor[5];
extern String standbyMode[3];
extern String loraOperationMode[5];
extern String WifiStandbyMode[3];
extern String transmitPriority[3];
extern String cssStyle[4];
extern String OledDisplayRotation[3];
extern String OledDisplayMode[3];
extern uint chipId;
extern String hname;
extern AsyncWebServer httpServer;
extern uint8_t getLMICtxChnl();
extern uint32_t getLMICseqnoUp();

void WebServerHandler();
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
bool installWebBundleFromTar(const String &bundlePath, String &installedVersion, String &errorMessage);
bool resolveOtaFirmwareForChannel(const String &channel, String &firmwareUrl, String &version, String *errorMessage, String *sha256, String *webFilesPath);
int compareOtaFirmwareVersions(const String &leftVersion, const String &rightVersion);
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage);
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256);
bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256, bool useMdsOtaEndpoint);
void cleanupStaleWebBundleArtifacts();
void maintainLocalOtaUpload();
void flushPendingRemoteOtaStatus();
String getCsrfToken();
bool isCsrfTokenValid(AsyncWebServerRequest *request);
bool requireCsrfToken(AsyncWebServerRequest *request);
String maskSecret(const String &secret);
bool queueManualLoraSend(String &message);
void buildManualLoraStatus(JsonDocument &response);
void printProgress(size_t prg, size_t sz);

#endif
