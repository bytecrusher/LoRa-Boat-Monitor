#ifndef UPDATE_RUNTIME_STATE_H
#define UPDATE_RUNTIME_STATE_H

#include <Arduino.h>

void initializeUpdateRuntimeState();

enum class MaintenanceOperation : uint8_t {
  None,
  RemoteOta,
  LocalFirmware,
  WebBundle,
  WebFiles,
  FormatFilesystem,
  SingleFileUpload,
  OtaDiagnostics,
  OtaMetadata
};

bool acquireMaintenanceOperation(MaintenanceOperation operation);
void releaseMaintenanceOperation(MaintenanceOperation operation);
MaintenanceOperation getMaintenanceOperation();
const char *maintenanceOperationName(MaintenanceOperation operation);

struct WebFilesUpdateSnapshot {
  size_t completed = 0;
  size_t total = 0;
  String currentName;
  String message;
  bool error = false;
  bool fatalError = false;
  uint8_t retryCount = 0;
  unsigned long startedMillis = 0;
  bool active = false;
  bool retrying = false;
  bool serverSupportKnown = false;
  bool serverSupportsInstalledFirmware = false;
  String storedWebFilesVersion;
  String storedWebFilesChannel;
};

void resetWebFilesUpdateState(size_t total, const String &currentName, const String &message);
void setWebFilesUpdateProgress(size_t completed, size_t total, const String &currentName, const String &message);
void setWebFilesUpdateMessage(const String &message);
void setWebFilesUpdateError(bool error, const String &message = "");
void setWebFilesUpdateFatalError(bool fatalError, const String &message = "");
void setWebFilesUpdateRetryCount(uint8_t retryCount);
uint8_t incrementWebFilesUpdateRetryCount();
void setWebFilesUpdateStartedMillis(unsigned long startedMillis);
void setWebFilesUpdateActivity(bool active, bool retrying = false);
void setWebFilesServerSupport(bool known, bool supported);
void setStoredWebFilesVersionInfo(const String &version, const String &channel);
WebFilesUpdateSnapshot getWebFilesUpdateSnapshot();

struct LocalUpdateSnapshot {
  bool inProgress = false;
  bool writerActive = false;
  unsigned long lastActivityMillis = 0;
};

void beginLocalUpdate(bool writerActive = false);
void touchLocalUpdate();
void setLocalUpdateWriterActive(bool writerActive);
void finishLocalUpdate();
LocalUpdateSnapshot getLocalUpdateSnapshot();

struct UploadResultSnapshot {
  int statusCode = 500;
  bool finished = false;
  bool succeeded = false;
  String response;
};

void resetWebBundleUploadResult(const String &defaultResponse);
void setWebBundleUploadResultState(int statusCode, const String &response);
UploadResultSnapshot getWebBundleUploadResult();
void resetSingleFileUploadResult(const String &defaultMessage);
void setSingleFileUploadResultState(bool succeeded, const String &message);
UploadResultSnapshot getSingleFileUploadResult();

struct RemoteOtaRequest {
  String url;
  String version;
  String sha256;
  String channel;
  bool forceReinstall = false;
  bool useMdsEndpoint = false;
};

struct RemoteOtaSnapshot {
  bool pending = false;
  bool inProgress = false;
  bool rebootRequired = false;
  RemoteOtaRequest request;
};

bool queueRemoteOtaRequest(const RemoteOtaRequest &request);
bool beginRemoteOtaRequest(RemoteOtaRequest &request);
void finishRemoteOtaRequest(bool rebootRequired);
void setRemoteOtaVersion(const String &version);
void setRemoteOtaRebootRequired(bool rebootRequired);
RemoteOtaSnapshot getRemoteOtaSnapshot();

struct OtaProgressSnapshot {
  bool active = false;
  bool success = false;
  size_t current = 0;
  size_t total = 0;
  String phase = "idle";
  String message;
};

void setOtaProgressState(const OtaProgressSnapshot &state);
OtaProgressSnapshot getOtaProgressSnapshot();

struct MdsTestSnapshot {
  bool running = false;
  bool complete = false;
  bool success = false;
  String message;
  String detail;
};

bool beginMdsTest();
void finishMdsTest(bool success, const String &message, const String &detail);
MdsTestSnapshot getMdsTestSnapshot();

struct OtaDiagnosticsSnapshot {
  bool running = false;
  bool complete = false;
  String json;
};

bool beginOtaDiagnostics();
void finishOtaDiagnostics(const String &json);
OtaDiagnosticsSnapshot getOtaDiagnosticsSnapshot();

struct OtaMetadataSnapshot {
  bool running = false;
  bool complete = false;
  String json;
};

bool beginOtaMetadata(const String &channel);
void finishOtaMetadata(const String &channel, const String &json);
OtaMetadataSnapshot getOtaMetadataSnapshot(const String &channel);

#endif
