#ifndef _func_webclient_H
#define _func_webclient_H

#include <Configuration.h>

bool DownloadFile(const char *fileName, char *fversion);
bool DownloadFilesFromWeb(char *fversion);
bool sendToMDS(configData actconf);
bool sendMdsDeviceEvent(configData actconf, const char *sensorName, float value1, float value2, float value3, float value4);
//StaticJsonDocument<200> collectJsonData();

#endif
