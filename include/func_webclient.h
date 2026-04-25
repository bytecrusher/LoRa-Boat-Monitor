#ifndef _func_webclient_H
#define _func_webclient_H

#include <Configuration.h>

bool DownloadFile(const char *fileName, const char *fversion);
bool DownloadFilesFromWeb();
bool sendToMDS(configData actconf);
bool sendMdsDeviceEvent(configData actconf, const char *sensorName);
//StaticJsonDocument<200> collectJsonData();

#endif
