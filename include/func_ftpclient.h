#ifndef _func_ftpclient_H
#define _func_ftpclient_H

void openFtpConnection(char *fversion);
void closeFtpConnection();
void listFTPdir();
void DownloadFilesFromFtp(char *fversion);
void getFileFromFtp(const char *fileName);

#endif
