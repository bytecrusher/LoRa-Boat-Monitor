#include "updateRuntimeState.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
StaticSemaphore_t stateMutexBuffer;
SemaphoreHandle_t stateMutex = nullptr;
WebFilesUpdateSnapshot webFilesState;
RemoteOtaSnapshot remoteOtaState;
OtaProgressSnapshot otaProgressState;
MdsTestSnapshot mdsTestState;
OtaDiagnosticsSnapshot otaDiagnosticsState;
OtaMetadataSnapshot betaMetadataState;
OtaMetadataSnapshot stableMetadataState;
LocalUpdateSnapshot localUpdateState;
UploadResultSnapshot webBundleUploadResult;
UploadResultSnapshot singleFileUploadResult;
MaintenanceOperation maintenanceOperation = MaintenanceOperation::None;
portMUX_TYPE stateMutexInitMux = portMUX_INITIALIZER_UNLOCKED;

SemaphoreHandle_t getStateMutex() {
  if (stateMutex == nullptr) {
    taskENTER_CRITICAL(&stateMutexInitMux);
    if (stateMutex == nullptr) {
      stateMutex = xSemaphoreCreateMutexStatic(&stateMutexBuffer);
    }
    taskEXIT_CRITICAL(&stateMutexInitMux);
  }
  return stateMutex;
}

class StateGuard {
public:
  StateGuard() {
    SemaphoreHandle_t mutex = getStateMutex();
    locked = mutex != nullptr && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
  }

  ~StateGuard() {
    if (locked) {
      xSemaphoreGive(getStateMutex());
    }
  }

  bool acquired() const { return locked; }

private:
  bool locked = false;
};
}

void initializeUpdateRuntimeState() {
  (void)getStateMutex();
}

bool acquireMaintenanceOperation(MaintenanceOperation operation) {
  if (operation == MaintenanceOperation::None) return false;
  StateGuard guard;
  if (!guard.acquired() || maintenanceOperation != MaintenanceOperation::None) return false;
  maintenanceOperation = operation;
  return true;
}

void releaseMaintenanceOperation(MaintenanceOperation operation) {
  StateGuard guard;
  if (!guard.acquired()) return;
  if (maintenanceOperation == operation) maintenanceOperation = MaintenanceOperation::None;
}

MaintenanceOperation getMaintenanceOperation() {
  StateGuard guard;
  return guard.acquired() ? maintenanceOperation : MaintenanceOperation::None;
}

const char *maintenanceOperationName(MaintenanceOperation operation) {
  switch (operation) {
    case MaintenanceOperation::RemoteOta: return "remote firmware update";
    case MaintenanceOperation::LocalFirmware: return "local firmware upload";
    case MaintenanceOperation::WebBundle: return "local web package upload";
    case MaintenanceOperation::WebFiles: return "web files update";
    case MaintenanceOperation::FormatFilesystem: return "filesystem format";
    case MaintenanceOperation::SingleFileUpload: return "single file upload";
    case MaintenanceOperation::OtaDiagnostics: return "OTA diagnostics";
    case MaintenanceOperation::OtaMetadata: return "OTA metadata check";
    default: return "none";
  }
}

void resetWebFilesUpdateState(size_t total, const String &currentName, const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  const bool active = webFilesState.active;
  const bool retrying = webFilesState.retrying;
  const String storedVersion = webFilesState.storedWebFilesVersion;
  const String storedChannel = webFilesState.storedWebFilesChannel;
  webFilesState = WebFilesUpdateSnapshot();
  webFilesState.active = active;
  webFilesState.retrying = retrying;
  webFilesState.storedWebFilesVersion = storedVersion;
  webFilesState.storedWebFilesChannel = storedChannel;
  webFilesState.total = total;
  webFilesState.currentName = currentName;
  webFilesState.message = message;
}

void setWebFilesUpdateProgress(size_t completed, size_t total, const String &currentName, const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.completed = completed;
  webFilesState.total = total;
  webFilesState.currentName = currentName;
  webFilesState.message = message;
}

void setWebFilesUpdateMessage(const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.message = message;
}

void setWebFilesUpdateError(bool error, const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.error = error;
  if (message.length() > 0) webFilesState.message = message;
}

void setWebFilesUpdateFatalError(bool fatalError, const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.fatalError = fatalError;
  if (message.length() > 0) webFilesState.message = message;
}

void setWebFilesUpdateRetryCount(uint8_t retryCount) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.retryCount = retryCount;
}

uint8_t incrementWebFilesUpdateRetryCount() {
  StateGuard guard;
  if (!guard.acquired()) return 0;
  return ++webFilesState.retryCount;
}

void setWebFilesUpdateStartedMillis(unsigned long startedMillis) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.startedMillis = startedMillis;
}

void setWebFilesUpdateActivity(bool active, bool retrying) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.active = active;
  webFilesState.retrying = retrying;
}

void setWebFilesServerSupport(bool known, bool supported) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.serverSupportKnown = known;
  webFilesState.serverSupportsInstalledFirmware = supported;
}

void setStoredWebFilesVersionInfo(const String &version, const String &channel) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webFilesState.storedWebFilesVersion = version;
  webFilesState.storedWebFilesChannel = channel;
}

WebFilesUpdateSnapshot getWebFilesUpdateSnapshot() {
  StateGuard guard;
  return guard.acquired() ? webFilesState : WebFilesUpdateSnapshot();
}

void beginLocalUpdate(bool writerActive) {
  StateGuard guard;
  if (!guard.acquired()) return;
  localUpdateState.inProgress = true;
  localUpdateState.writerActive = writerActive;
  localUpdateState.lastActivityMillis = millis();
}

void touchLocalUpdate() {
  StateGuard guard;
  if (!guard.acquired() || !localUpdateState.inProgress) return;
  localUpdateState.lastActivityMillis = millis();
}

void setLocalUpdateWriterActive(bool writerActive) {
  StateGuard guard;
  if (!guard.acquired()) return;
  localUpdateState.writerActive = writerActive;
  if (localUpdateState.inProgress) localUpdateState.lastActivityMillis = millis();
}

void finishLocalUpdate() {
  StateGuard guard;
  if (!guard.acquired()) return;
  localUpdateState = LocalUpdateSnapshot();
}

LocalUpdateSnapshot getLocalUpdateSnapshot() {
  StateGuard guard;
  return guard.acquired() ? localUpdateState : LocalUpdateSnapshot();
}

void resetWebBundleUploadResult(const String &defaultResponse) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webBundleUploadResult = UploadResultSnapshot();
  webBundleUploadResult.response = defaultResponse;
}

void setWebBundleUploadResultState(int statusCode, const String &response) {
  StateGuard guard;
  if (!guard.acquired()) return;
  webBundleUploadResult.statusCode = statusCode;
  webBundleUploadResult.finished = true;
  webBundleUploadResult.succeeded = statusCode >= 200 && statusCode < 300;
  webBundleUploadResult.response = response;
}

UploadResultSnapshot getWebBundleUploadResult() {
  StateGuard guard;
  return guard.acquired() ? webBundleUploadResult : UploadResultSnapshot();
}

void resetSingleFileUploadResult(const String &defaultMessage) {
  StateGuard guard;
  if (!guard.acquired()) return;
  singleFileUploadResult = UploadResultSnapshot();
  singleFileUploadResult.response = defaultMessage;
}

void setSingleFileUploadResultState(bool succeeded, const String &message) {
  StateGuard guard;
  if (!guard.acquired()) return;
  singleFileUploadResult.statusCode = succeeded ? 200 : 400;
  singleFileUploadResult.finished = true;
  singleFileUploadResult.succeeded = succeeded;
  singleFileUploadResult.response = message;
}

UploadResultSnapshot getSingleFileUploadResult() {
  StateGuard guard;
  return guard.acquired() ? singleFileUploadResult : UploadResultSnapshot();
}

bool queueRemoteOtaRequest(const RemoteOtaRequest &request) {
  StateGuard guard;
  if (!guard.acquired() || remoteOtaState.pending || remoteOtaState.inProgress) return false;
  remoteOtaState.request = request;
  remoteOtaState.pending = true;
  remoteOtaState.rebootRequired = false;
  return true;
}

bool beginRemoteOtaRequest(RemoteOtaRequest &request) {
  StateGuard guard;
  if (!guard.acquired() || !remoteOtaState.pending || remoteOtaState.inProgress) return false;
  remoteOtaState.pending = false;
  remoteOtaState.inProgress = true;
  remoteOtaState.rebootRequired = false;
  request = remoteOtaState.request;
  return true;
}

void finishRemoteOtaRequest(bool rebootRequired) {
  StateGuard guard;
  if (!guard.acquired()) return;
  remoteOtaState.inProgress = false;
  remoteOtaState.rebootRequired = rebootRequired;
  remoteOtaState.request.sha256 = "";
  remoteOtaState.request.channel = "";
  remoteOtaState.request.forceReinstall = false;
  remoteOtaState.request.useMdsEndpoint = false;
}

void setRemoteOtaVersion(const String &version) {
  StateGuard guard;
  if (!guard.acquired()) return;
  remoteOtaState.request.version = version;
}

void setRemoteOtaRebootRequired(bool rebootRequired) {
  StateGuard guard;
  if (!guard.acquired()) return;
  remoteOtaState.rebootRequired = rebootRequired;
}

RemoteOtaSnapshot getRemoteOtaSnapshot() {
  StateGuard guard;
  return guard.acquired() ? remoteOtaState : RemoteOtaSnapshot();
}

void setOtaProgressState(const OtaProgressSnapshot &state) {
  StateGuard guard;
  if (!guard.acquired()) return;
  otaProgressState = state;
}

OtaProgressSnapshot getOtaProgressSnapshot() {
  StateGuard guard;
  return guard.acquired() ? otaProgressState : OtaProgressSnapshot();
}

bool beginMdsTest() {
  StateGuard guard;
  if (!guard.acquired() || mdsTestState.running) return false;
  mdsTestState = MdsTestSnapshot();
  mdsTestState.running = true;
  mdsTestState.message = "MDS test upload is running.";
  return true;
}

void finishMdsTest(bool success, const String &message, const String &detail) {
  StateGuard guard;
  if (!guard.acquired()) return;
  mdsTestState.running = false;
  mdsTestState.complete = true;
  mdsTestState.success = success;
  mdsTestState.message = message;
  mdsTestState.detail = detail;
}

MdsTestSnapshot getMdsTestSnapshot() {
  StateGuard guard;
  return guard.acquired() ? mdsTestState : MdsTestSnapshot();
}

bool beginOtaDiagnostics() {
  StateGuard guard;
  if (!guard.acquired() || otaDiagnosticsState.running) return false;
  otaDiagnosticsState.running = true;
  otaDiagnosticsState.complete = false;
  return true;
}

void finishOtaDiagnostics(const String &json) {
  StateGuard guard;
  if (!guard.acquired()) return;
  otaDiagnosticsState.running = false;
  otaDiagnosticsState.complete = true;
  otaDiagnosticsState.json = json;
}

OtaDiagnosticsSnapshot getOtaDiagnosticsSnapshot() {
  StateGuard guard;
  return guard.acquired() ? otaDiagnosticsState : OtaDiagnosticsSnapshot();
}

namespace {
OtaMetadataSnapshot *otaMetadataStateForChannel(const String &channel) {
  if (channel == "beta") return &betaMetadataState;
  if (channel == "stable" || channel == "release") return &stableMetadataState;
  return nullptr;
}
}

bool beginOtaMetadata(const String &channel) {
  StateGuard guard;
  OtaMetadataSnapshot *state = otaMetadataStateForChannel(channel);
  if (!guard.acquired() || state == nullptr || state->running) return false;
  state->running = true;
  state->complete = false;
  return true;
}

void finishOtaMetadata(const String &channel, const String &json) {
  StateGuard guard;
  OtaMetadataSnapshot *state = otaMetadataStateForChannel(channel);
  if (!guard.acquired() || state == nullptr) return;
  state->running = false;
  state->complete = true;
  state->json = json;
}

OtaMetadataSnapshot getOtaMetadataSnapshot(const String &channel) {
  StateGuard guard;
  OtaMetadataSnapshot *state = otaMetadataStateForChannel(channel);
  return guard.acquired() && state != nullptr ? *state : OtaMetadataSnapshot();
}
