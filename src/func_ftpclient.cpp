#include "func_ftpclient.h"
#include "func_myFunctions.h"

void DownloadFilesFromFtp(char *fversion)
{
  DebugPrintln(2, "Legacy FTP web-file download is disabled. Use HTTPS update files instead.");
}

void openFtpConnection(char *fversion)
{
  DebugPrintln(2, "Legacy FTP connection is disabled.");
}

void closeFtpConnection()
{
}

void listFTPdir()
{
  DebugPrintln(2, "Legacy FTP directory listing is disabled.");
}

void getFileFromFtp(const char *fileName)
{
  DebugPrintln(2, "Legacy FTP file download is disabled.");
}
