#ifndef _func_webclient_H
#define _func_webclient_H

#include <Arduino.h>
#include <Configuration.h>

bool DownloadFile(const char *fileName, const char *fversion, const String &expectedSha256 = "");
bool DownloadFilesFromWeb();
bool sendToMDS(configData actconf);
bool sendMdsDeviceEvent(configData actconf, const char *sensorName);
//StaticJsonDocument<200> collectJsonData();

#endif
