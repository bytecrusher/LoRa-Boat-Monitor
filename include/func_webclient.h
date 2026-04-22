#ifndef _func_webclient_H
#define _func_webclient_H

#include <Configuration.h>

bool DownloadFile(const char *fileName, char *fversion);
bool DownloadFilesFromWeb(char *fversion);
void sendToMDS(configData actconf);
//StaticJsonDocument<200> collectJsonData();

#endif
