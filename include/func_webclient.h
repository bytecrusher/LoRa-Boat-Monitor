#ifndef _func_webclient_H
#define _func_webclient_H

#include <Arduino.h>
#include <Configuration.h>

bool DownloadFilesFromWeb();
void recoverInterruptedWebFilesUpdate();
bool sendToMDS(configData &actconf);
bool sendMdsDeviceEvent(const configData &actconf, const char *sensorName);
bool sendMdsOtaStatus(const configData &actconf, const char *phase, int percent, const String &targetVersion, const String &message);
String getLastMdsStatus();
//StaticJsonDocument<200> collectJsonData();

#endif
