#include "func_webServerHandler.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include "func_webclient.h"
#include "updateRuntimeState.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");

String csrfToken = "";

String getCsrfToken() {
  if (csrfToken.length() == 0) {
    csrfToken = String(esp_random(), HEX) + String(esp_random(), HEX) + String(esp_random(), HEX);
  }

  return csrfToken;
}

bool isCsrfTokenValid(AsyncWebServerRequest *request) {
  const String expectedToken = getCsrfToken();
  if (request->hasHeader("X-CSRF-Token") && request->header("X-CSRF-Token") == expectedToken) {
    return true;
  }

  if (request->hasParam("csrf", true) && request->getParam("csrf", true)->value() == expectedToken) {
    return true;
  }

  if (request->hasParam("csrf", true, true) && request->getParam("csrf", true, true)->value() == expectedToken) {
    return true;
  }

  return false;
}

bool requireCsrfToken(AsyncWebServerRequest *request) {
  if (isCsrfTokenValid(request)) {
    return true;
  }

  request->send(403, "application/json", "{\"status\":\"error\",\"message\":\"Invalid CSRF token.\"}");
  return false;
}

String maskSecret(const String &secret) {
  if (secret.length() == 0) {
    return "";
  }

  if (secret.length() <= 4) {
    return "****";
  }

  return secret.substring(0, 2) + "****" + secret.substring(secret.length() - 2);
}

namespace {
String buildOtaResponse(const char *status, const String &message, bool rebooting, bool checkWebFiles, bool backupSaved);
String buildOtaProgressJson();
String buildUpdateFilesProgressJson();
void startOtaProgress(const String &phase, size_t totalBytes, const String &message);
void updateOtaProgress(const String &phase, size_t currentBytes, size_t totalBytes, const String &message);
void finishOtaProgress(bool success, const String &message);
String readLittleFsTextFile(const char *path);
void sendLittleFsTextFile(AsyncWebServerRequest *request, const char *path, const char *contentType, const char *cacheControl = "no-store, max-age=0");
bool queueWebFilesDownloadTask(String &errorMessage);
String normalizeManifestChannel(const String &requestedChannel);
String channelDisplayLabel(const String &channel);
void cleanupStaleWebBundleArtifactsInternal(const String &preservePath = "");
bool resolveFirmwareFromManifest(const String &channel, String &firmwareUrl, String &version, String *errorMessage = nullptr, String *sha256 = nullptr, String *webFilesPath = nullptr);
int compareFirmwareVersions(const String &leftVersion, const String &rightVersion);
constexpr char WEB_BUNDLE_UPLOAD_PATH[] = "/webbundle-upload.tar";
constexpr char WEB_BUNDLE_JOURNAL_PATH[] = "/.webbundle-installing";
constexpr size_t MAX_WEB_BUNDLE_UPLOAD_SIZE = 1024 * 1024;
constexpr size_t MAX_WEB_BUNDLE_FILE_COUNT = 40;

struct PendingWebBundleFile {
  String tempPath;
  String targetPath;
  String backupPath;
  bool hadOriginal = false;
};

struct WifiPrioritySnapshot {
  int corder1;
  int corder2;
  int corder3;
};

int webBundleUploadStatusCode = 500;
String webBundleUploadResponse = "";
bool webBundleUploadFinished = false;
bool webBundleUploadFailed = false;
bool singleFileUploadFinished = false;
bool singleFileUploadSucceeded = false;
String singleFileUploadMessage;
TaskHandle_t webFilesDownloadTaskHandle = nullptr;
TaskHandle_t mdsTestTaskHandle = nullptr;
TaskHandle_t otaDiagnosticsTaskHandle = nullptr;
TaskHandle_t otaMetadataTaskHandle = nullptr;
configData mdsTestConfig;
String otaMetadataTaskChannel;
unsigned long localOtaLastActivityMillis = 0;
bool localOtaUpdateWriterActive = false;
constexpr unsigned long LOCAL_OTA_STALL_TIMEOUT_MS = 30000UL;

void mdsTestTask(void *parameter) {
  (void)parameter;
  DebugPrintln(3, "Manual Test MDS Upload started in background task");
  const bool success = sendToMDS(mdsTestConfig);
  finishMdsTest(success,
                success ? "MDS test upload sent successfully." : "MDS test upload failed.",
                getLastMdsStatus());
  mdsTestTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool queueMdsTest(String &errorMessage) {
  if (!beginMdsTest()) {
    errorMessage = "An MDS test upload is already running.";
    return false;
  }

  mdsTestConfig = actconf;

  const BaseType_t created = xTaskCreate(mdsTestTask, "mdsTest", 12288, nullptr, 1, &mdsTestTaskHandle);
  if (created != pdPASS) {
    mdsTestTaskHandle = nullptr;
    errorMessage = "Could not start MDS test task.";
    finishMdsTest(false, errorMessage, "Task allocation failed.");
    return false;
  }
  return true;
}

void resetWebBundleUploadState() {
  webBundleUploadStatusCode = 500;
  webBundleUploadResponse = buildOtaResponse("error", "Web package upload did not complete.", false, false, false);
  webBundleUploadFinished = false;
  webBundleUploadFailed = false;
}

void setWebBundleUploadResult(int statusCode, const char *status, const String &message, bool backupSaved) {
  webBundleUploadStatusCode = statusCode;
  webBundleUploadResponse = buildOtaResponse(status, message, false, false, backupSaved);
  webBundleUploadFinished = true;
  webBundleUploadFailed = statusCode < 200 || statusCode >= 300;
}

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); i++) {
    switch (value[i]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[i]; break;
    }
  }
  return escaped;
}

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); i++) {
    switch (value[i]) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped += value[i]; break;
    }
  }
  return escaped;
}

String trimNullTerminatedField(const uint8_t *field, size_t length) {
  size_t actualLength = 0;
  while (actualLength < length && field[actualLength] != '\0') {
    actualLength++;
  }

  String value = "";
  value.reserve(actualLength);
  for (size_t i = 0; i < actualLength; i++) {
    value += char(field[i]);
  }
  value.trim();
  return value;
}

unsigned long parseTarOctal(const uint8_t *field, size_t length) {
  unsigned long value = 0;
  for (size_t i = 0; i < length; i++) {
    const char c = char(field[i]);
    if (c == '\0' || c == ' ') {
      continue;
    }
    if (c < '0' || c > '7') {
      break;
    }
    value = (value << 3) + static_cast<unsigned long>(c - '0');
  }
  return value;
}

bool isZeroTarBlock(const uint8_t *block, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (block[i] != 0) {
      return false;
    }
  }
  return true;
}

bool isValidTarHeader(const uint8_t *header) {
  if (memcmp(header + 257, "ustar", 5) != 0) {
    return false;
  }

  const unsigned long expectedChecksum = parseTarOctal(header + 148, 8);
  unsigned long actualChecksum = 0;
  for (size_t i = 0; i < 512; ++i) {
    actualChecksum += (i >= 148 && i < 156) ? static_cast<uint8_t>(' ') : header[i];
  }
  return expectedChecksum == actualChecksum;
}

bool readFileExactly(File &file, uint8_t *buffer, size_t length) {
  return file.read(buffer, length) == length;
}

bool skipFileBytes(File &file, size_t length) {
  uint8_t buffer[256];
  size_t remaining = length;
  while (remaining > 0) {
    const size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
    if (file.read(buffer, chunk) != chunk) {
      return false;
    }
    remaining -= chunk;
  }
  return true;
}

bool isAllowedWebBundleFile(const String &filename) {
  static const char *allowedFiles[] = {
    "app.js",
    "common.css",
    "common.js",
    "css_black.css",
    "css_red.css",
    "css_white.css",
    "devinfo.html",
    "error.html",
    "favicon.ico",
    "filesystem.html",
    "filesystem.js",
    "firmware-page.js",
    "firmware.html",
    "gauge.min.js",
    "header.html",
    "header.js",
    "index.html",
    "index.js",
    "lora.html",
    "lora.js",
    "restart.html",
    "restart.js",
    "sensorv.html",
    "sensorv.js",
    "settings-page.css",
    "settings.html",
    "settings.js",
    "webfiles-version.txt"
  };

  for (const char *allowedFile : allowedFiles) {
    if (filename == String(allowedFile)) {
      return true;
    }
  }

  return false;
}

bool pendingWebBundleContains(const PendingWebBundleFile *files, size_t count, const String &targetPath) {
  for (size_t i = 0; i < count; ++i) {
    if (files[i].targetPath == targetPath) {
      return true;
    }
  }
  return false;
}

String webBundleTempPath(const String &filename) {
  return "/." + filename + ".bundle";
}

void recoverInterruptedWebBundleInstall() {
  if (!LittleFS.exists(WEB_BUNDLE_JOURNAL_PATH)) return;

  File journal = LittleFS.open(WEB_BUNDLE_JOURNAL_PATH, FILE_READ);
  if (!journal) {
    LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
    return;
  }
  while (journal.available()) {
    String entry = journal.readStringUntil('\n');
    entry.trim();
    const int separator = entry.indexOf('|');
    if (separator != 1) continue;
    const bool hadOriginal = entry.charAt(0) == '1';
    const String targetPath = entry.substring(separator + 1);
    const String filename = targetPath.startsWith("/") ? targetPath.substring(1) : targetPath;
    if (!isAllowedWebBundleFile(filename)) continue;
    const String backupPath = targetPath + ".rollback";
    if (hadOriginal) {
      if (LittleFS.exists(backupPath)) {
        LittleFS.remove(targetPath);
        LittleFS.rename(backupPath, targetPath);
      }
    } else {
      LittleFS.remove(targetPath);
      LittleFS.remove(backupPath);
    }
  }
  journal.close();
  LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
  DebugPrintln(2, "Recovered interrupted web package installation");
}

void cleanupStaleWebBundleArtifactsInternal(const String &preservePath) {
  recoverInterruptedWebBundleInstall();
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String path = file.name();
    String normalizedPath = path;
    if (!normalizedPath.startsWith("/")) {
      normalizedPath = "/" + normalizedPath;
    }
    const bool isPreservedPath = preservePath.length() > 0 && normalizedPath == preservePath;
    const bool isBundleTemp = normalizedPath.startsWith("/.") && normalizedPath.endsWith(".bundle");
    const bool isUploadTar = normalizedPath == WEB_BUNDLE_UPLOAD_PATH;
    const bool isRollback = normalizedPath.endsWith(".rollback");

    file.close();

    if (!isPreservedPath && isRollback) {
      String targetPath = normalizedPath.substring(0, normalizedPath.length() - String(".rollback").length());
      if (LittleFS.exists(targetPath)) {
        LittleFS.remove(normalizedPath);
      } else {
        LittleFS.rename(normalizedPath, targetPath);
      }
    } else if (!isPreservedPath && (isBundleTemp || isUploadTar)) {
      LittleFS.remove(normalizedPath);
    }

    file = root.openNextFile();
  }
}
void cleanupPendingWebBundleFiles(PendingWebBundleFile *files, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (files[i].tempPath.length() > 0 && LittleFS.exists(files[i].tempPath)) {
      LittleFS.remove(files[i].tempPath);
    }
  }
}

String readLittleFsTextFile(const char *path) {
  File file = LittleFS.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    DebugPrintln(1, String("Failed to open LittleFS file: ") + path);
    return "";
  }

  String content;
  const size_t fileSize = file.size();
  if (fileSize > 0) {
    content.reserve(fileSize);
  }

  while (file.available()) {
    content += char(file.read());
  }

  file.close();
  return content;
}

void sendLittleFsTextFile(AsyncWebServerRequest *request, const char *path, const char *contentType, const char *cacheControl) {
  if (!LittleFS.exists(path)) {
    request->send(404, "text/plain", "The content you are looking for was not found.");
    return;
  }

  File file = LittleFS.open(path, FILE_READ);
  if (!file || file.isDirectory() || file.size() == 0) {
    if (file) {
      file.close();
    }
    request->send(500, "text/plain", "Failed to stream content from LittleFS.");
    return;
  }
  file.close();

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType, false);
  response->addHeader("Cache-Control", cacheControl);
  request->send(response);
}

void webFilesDownloadTask(void *parameter) {
  (void)parameter;

  runDownloadingFilesStatus = true;
  setWebFilesUpdateError(false, "Downloading web files from server.");
  setWebFilesUpdateFatalError(false);

  bool success = false;
  if (WiFi.status() != WL_CONNECTED) {
    setWebFilesUpdateError(true, "No WiFi connection available for web files download.");
  } else {
    success = DownloadFilesFromWeb();
    if (success) {
      setWebFilesUpdateError(false, "Web files updated successfully.");
      setWebFilesUpdateRetryCount(0);
      writeDisplayStatusScreen("Web files", "Update complete", String(actconf.fversion));
    } else {
      const WebFilesUpdateSnapshot state = getWebFilesUpdateSnapshot();
      setWebFilesUpdateError(true, state.message.length() == 0 ? "Web files download failed." : state.message);
      writeDisplayStatusScreen("Web files", "Download failed", "Retry manual");
    }
  }

  runDownloadingFiles = false;
  runDownloadingFilesStatus = false;
  releaseMaintenanceOperation(MaintenanceOperation::WebFiles);
  webFilesDownloadTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool queueWebFilesDownloadTask(String &errorMessage) {
  if (webFilesDownloadTaskHandle != nullptr || runDownloadingFilesStatus || runDownloadingFiles) {
    errorMessage = "Web files download is already running.";
    return false;
  }
  if (!acquireMaintenanceOperation(MaintenanceOperation::WebFiles)) {
    errorMessage = String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".";
    return false;
  }

  resetWebFilesUpdateState(0, "", "Web files download queued.");
  setWebFilesUpdateStartedMillis(millis());
  runDownloadingFiles = false;
  runDownloadingFilesStatus = true;

  BaseType_t created = xTaskCreate(
    webFilesDownloadTask,
    "webFilesDownload",
    12288,
    nullptr,
    1,
    &webFilesDownloadTaskHandle
  );

  if (created != pdPASS) {
    webFilesDownloadTaskHandle = nullptr;
    runDownloadingFilesStatus = false;
    errorMessage = "Could not start web files download task.";
    setWebFilesUpdateError(true, errorMessage);
    releaseMaintenanceOperation(MaintenanceOperation::WebFiles);
    return false;
  }

  return true;
}

bool queueRemoteOtaUpdateFromMdsEndpoint(const String &source, bool forceReinstall, String &errorMessage) {
  const RemoteOtaSnapshot currentState = getRemoteOtaSnapshot();
  if (currentState.pending || currentState.inProgress) {
    errorMessage = "A remote OTA update is already running.";
    return false;
  }
  if (!acquireMaintenanceOperation(MaintenanceOperation::RemoteOta)) {
    errorMessage = String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".";
    return false;
  }

  String endpoint = String(actconf.mdsOtaUrl);
  endpoint.trim();
  String normalizedEndpoint;
  if (!normalizeMdsOtaEndpoint(endpoint, normalizedEndpoint)) {
    errorMessage = "MDS OTA endpoint is not configured.";
    releaseMaintenanceOperation(MaintenanceOperation::RemoteOta);
    return false;
  }

  String channel = getFirmwareReleaseChannel();
  if (source == "mds-beta") {
    channel = "beta";
  } else if (source == "mds-release" || source == "mds-stable") {
    channel = "stable";
  }

  RemoteOtaRequest otaRequest;
  otaRequest.url = normalizedEndpoint;
  otaRequest.channel = channel;
  otaRequest.forceReinstall = forceReinstall;
  otaRequest.useMdsEndpoint = true;
  if (!queueRemoteOtaRequest(otaRequest)) {
    errorMessage = "A remote OTA update is already running.";
    releaseMaintenanceOperation(MaintenanceOperation::RemoteOta);
    return false;
  }
  standbySleepBlockedUntilMillis = millis() + 180000UL;
  startOtaProgress("queued", 0, "Remote firmware update queued on device...");
  DebugPrintln(3, "Remote firmware update queued via MDS endpoint for channel: " + channel);
  return true;
}

String normalizeManifestChannel(const String &requestedChannel) {
  String channel = requestedChannel;
  channel.trim();
  channel.toLowerCase();

  if (channel == "release" || channel == "stable") {
    return "stable";
  }
  if (channel == "beta") {
    return "beta";
  }

  return "";
}

String channelDisplayLabel(const String &channel) {
  return normalizeManifestChannel(channel) == "stable" ? "Release" : "Beta";
}

bool installWebBundleFromTarInternal(const String &bundlePath, String &installedVersion, String &errorMessage) {
  DebugPrintln(3, "Installing local web package from TAR");
  cleanupStaleWebBundleArtifactsInternal(bundlePath);
  File bundleFile = LittleFS.open(bundlePath, FILE_READ);
  if (!bundleFile) {
    errorMessage = "Uploaded web package could not be opened.";
    return false;
  }

  PendingWebBundleFile pendingFiles[MAX_WEB_BUNDLE_FILE_COUNT];
  size_t pendingCount = 0;
  installedVersion = "";
  uint8_t header[512];
  uint8_t buffer[512];
  bool foundSupportedFile = false;

  while (true) {
    if (!readFileExactly(bundleFile, header, sizeof(header))) {
      errorMessage = "Web package header is incomplete.";
      cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
      bundleFile.close();
      return false;
    }

    if (isZeroTarBlock(header, sizeof(header))) {
      break;
    }
    if (!isValidTarHeader(header)) {
      errorMessage = "Web package contains an invalid TAR header.";
      cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
      bundleFile.close();
      return false;
    }

    String fileName = trimNullTerminatedField(header, 100);
    String prefix = trimNullTerminatedField(header + 345, 155);
    if (prefix.length() > 0) {
      fileName = prefix + "/" + fileName;
    }
    if (fileName.startsWith("./")) {
      fileName.remove(0, 2);
    }
    while (fileName.startsWith("/")) {
      fileName.remove(0, 1);
    }

    const unsigned long fileSize = parseTarOctal(header + 124, 12);
    const char typeFlag = char(header[156]);
    const bool isRegularFile = (typeFlag == '\0' || typeFlag == '0');

    if (!isRegularFile) {
      if (!skipFileBytes(bundleFile, fileSize)) {
        errorMessage = "Failed to skip unsupported web package entry.";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }
    } else if (fileName.indexOf('/') >= 0 || !isAllowedWebBundleFile(fileName)) {
      if (!skipFileBytes(bundleFile, fileSize)) {
        errorMessage = "Failed to skip unsupported web package file.";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }
    } else {
      DebugPrintln(3, "Extracting web package file: " + fileName);
      const String targetPath = "/" + fileName;
      if (pendingWebBundleContains(pendingFiles, pendingCount, targetPath)) {
        errorMessage = "Web package contains a duplicate file: " + fileName + ".";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }
      if (fileSize == 0) {
        errorMessage = "Web package contains an empty file: " + fileName + ".";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }

      if (pendingCount >= MAX_WEB_BUNDLE_FILE_COUNT) {
        errorMessage = "Web package contains too many files.";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }

      foundSupportedFile = true;
      const String tempPath = webBundleTempPath(fileName);
      if (LittleFS.exists(tempPath)) {
        LittleFS.remove(tempPath);
      }

      File outputFile = LittleFS.open(tempPath, FILE_WRITE);
      if (!outputFile) {
        errorMessage = "Could not create temporary web package file.";
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        bundleFile.close();
        return false;
      }

      String fileText = "";
      if (fileName == "webfiles-version.txt") {
        fileText.reserve(fileSize);
      }

      size_t remaining = fileSize;
      while (remaining > 0) {
        const size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
        if (bundleFile.read(buffer, chunk) != chunk) {
          outputFile.close();
          LittleFS.remove(tempPath);
          errorMessage = "Web package file is truncated.";
          cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
          bundleFile.close();
          return false;
        }
        if (outputFile.write(buffer, chunk) != chunk) {
          outputFile.close();
          LittleFS.remove(tempPath);
          errorMessage = "Could not write extracted web package file.";
          cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
          bundleFile.close();
          return false;
        }
        if (fileName == "webfiles-version.txt") {
          for (size_t i = 0; i < chunk; i++) {
            fileText += char(buffer[i]);
          }
        }
        remaining -= chunk;
      }
      outputFile.close();

      if (fileName == "webfiles-version.txt") {
        fileText.trim();
        installedVersion = fileText;
      }

      pendingFiles[pendingCount].tempPath = tempPath;
      pendingFiles[pendingCount].targetPath = targetPath;
      pendingCount++;
    }

    const size_t padding = (512 - (fileSize % 512)) % 512;
    if (padding > 0 && !skipFileBytes(bundleFile, padding)) {
      errorMessage = "Failed to skip web package padding.";
      cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
      bundleFile.close();
      return false;
    }
  }

  bundleFile.close();

  if (!foundSupportedFile || pendingCount == 0) {
    errorMessage = "Web package contains no supported web interface files.";
    cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
    return false;
  }

  if (installedVersion.length() == 0) {
    errorMessage = "Web package is missing webfiles-version.txt.";
    cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
    return false;
  }

  String packageVersion = installedVersion;
  String packageChannel = getFirmwareReleaseChannel();
  const int packageSeparator = packageVersion.indexOf('|');
  if (packageSeparator >= 0) {
    packageChannel = packageVersion.substring(packageSeparator + 1);
    packageVersion = packageVersion.substring(0, packageSeparator);
  }
  packageVersion.trim();
  packageChannel.trim();
  packageChannel.toLowerCase();
  const String packageVersionTag = buildWebFilesVersionTag(packageVersion.c_str(), packageChannel.c_str());
  const String expectedVersionTag = buildWebFilesVersionTag(actconf.fversion);
  if (packageVersionTag != expectedVersionTag) {
    errorMessage = "Web package " + packageVersionTag + " does not match installed firmware " + expectedVersionTag + ".";
    cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
    return false;
  }

  static const char *requiredFiles[] = {
    "common.css", "common.js", "header.html", "header.js",
    "index.html", "index.js", "firmware.html", "firmware-page.js",
    "filesystem.html", "filesystem.js", "settings.html", "settings.js", "sensorv.html",
    "sensorv.js", "lora.html", "lora.js", "restart.html", "restart.js",
    "webfiles-version.txt"
  };
  for (const char *requiredFile : requiredFiles) {
    if (!pendingWebBundleContains(pendingFiles, pendingCount, "/" + String(requiredFile))) {
      errorMessage = "Web package is incomplete; missing " + String(requiredFile) + ".";
      cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
      return false;
    }
  }

  File installJournal = LittleFS.open(WEB_BUNDLE_JOURNAL_PATH, FILE_WRITE);
  if (!installJournal) {
    errorMessage = "Could not create web package installation journal.";
    cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
    return false;
  }
  for (size_t i = 0; i < pendingCount; ++i) {
    pendingFiles[i].backupPath = pendingFiles[i].targetPath + ".rollback";
    pendingFiles[i].hadOriginal = LittleFS.exists(pendingFiles[i].targetPath);
    installJournal.println(String(pendingFiles[i].hadOriginal ? "1|" : "0|") + pendingFiles[i].targetPath);
  }
  installJournal.close();

  // Move existing files aside first. If any later rename fails, restore the
  // complete previous set instead of leaving a partially updated interface.
  for (size_t i = 0; i < pendingCount; ++i) {
    LittleFS.remove(pendingFiles[i].backupPath);
    if (pendingFiles[i].hadOriginal) {
      if (!LittleFS.rename(pendingFiles[i].targetPath, pendingFiles[i].backupPath)) {
        errorMessage = "Could not prepare transactional web package install.";
        for (size_t j = 0; j < i; ++j) {
          if (pendingFiles[j].hadOriginal) {
            LittleFS.rename(pendingFiles[j].backupPath, pendingFiles[j].targetPath);
          }
        }
        cleanupPendingWebBundleFiles(pendingFiles, pendingCount);
        LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
        return false;
      }
    }
  }

  size_t installedCount = 0;
  for (; installedCount < pendingCount; ++installedCount) {
    if (!LittleFS.rename(pendingFiles[installedCount].tempPath, pendingFiles[installedCount].targetPath)) {
      errorMessage = "Failed to install extracted web package files; previous files restored.";
      break;
    }
  }
  if (installedCount != pendingCount) {
    for (size_t i = 0; i < installedCount; ++i) {
      LittleFS.remove(pendingFiles[i].targetPath);
    }
    for (size_t i = 0; i < pendingCount; ++i) {
      if (pendingFiles[i].hadOriginal) {
        LittleFS.rename(pendingFiles[i].backupPath, pendingFiles[i].targetPath);
      }
    }
    cleanupPendingWebBundleFiles(pendingFiles + installedCount, pendingCount - installedCount);
    LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
    return false;
  }

  if (!saveWebFilesVersion(packageVersion.c_str())) {
    errorMessage = "Web package installed, but version marker could not be updated.";
    for (size_t i = 0; i < pendingCount; ++i) {
      LittleFS.remove(pendingFiles[i].targetPath);
      if (pendingFiles[i].hadOriginal) {
        LittleFS.rename(pendingFiles[i].backupPath, pendingFiles[i].targetPath);
      }
    }
    LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
    return false;
  }

  const String storedVersion = getStoredWebFilesVersion();
  if (storedVersion != expectedVersionTag) {
    errorMessage = "Web package installed, but version marker verification failed.";
    DebugPrintln(1, "Web package version marker mismatch. Expected: " + expectedVersionTag + ", got: " + storedVersion);
    for (size_t i = 0; i < pendingCount; ++i) {
      LittleFS.remove(pendingFiles[i].targetPath);
      if (pendingFiles[i].hadOriginal) {
        LittleFS.rename(pendingFiles[i].backupPath, pendingFiles[i].targetPath);
      }
    }
    LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
    return false;
  }

  // Removing the journal is the atomic commit point. Any reset before this
  // line restores the complete previous generation on the next boot.
  LittleFS.remove(WEB_BUNDLE_JOURNAL_PATH);
  for (size_t i = 0; i < pendingCount; ++i) {
    if (pendingFiles[i].hadOriginal) {
      LittleFS.remove(pendingFiles[i].backupPath);
    }
  }

  DebugPrintln(3, "Local web package installed successfully. Stored version: " + storedVersion);
  cleanupStaleWebBundleArtifactsInternal();

  return true;
}

int clampConfigInt(int value, int fallback, int minValue, int maxValue) {
  if (value < minValue || value > maxValue) {
    return fallback;
  }

  return value;
}

int getWifiPriorityFromSnapshot(const WifiPrioritySnapshot &snapshot, int slot) {
  if (slot == 1) return snapshot.corder1;
  if (slot == 2) return snapshot.corder2;
  if (slot == 3) return snapshot.corder3;
  return 0;
}

int getWifiPriorityFromConfig(int slot) {
  if (slot == 1) return actconf.corder1;
  if (slot == 2) return actconf.corder2;
  if (slot == 3) return actconf.corder3;
  return 0;
}

void setWifiPriorityInConfig(int slot, int priority) {
  if (slot == 1) actconf.corder1 = priority;
  if (slot == 2) actconf.corder2 = priority;
  if (slot == 3) actconf.corder3 = priority;
}

void ensureUniqueWifiPriorities(const WifiPrioritySnapshot &previousPriorities) {
  for (int changedSlot = 1; changedSlot <= 3; changedSlot++) {
    const int previousPriority = getWifiPriorityFromSnapshot(previousPriorities, changedSlot);
    const int newPriority = getWifiPriorityFromConfig(changedSlot);
    if (newPriority <= 0 || newPriority == previousPriority) {
      continue;
    }

    for (int otherSlot = 1; otherSlot <= 3; otherSlot++) {
      if (otherSlot == changedSlot || getWifiPriorityFromConfig(otherSlot) != newPriority) {
        continue;
      }

      setWifiPriorityInConfig(otherSlot, previousPriority);
      DebugPrint(3, "Swapped duplicate WiFi priority from slot ");
      DebugPrint(3, changedSlot);
      DebugPrint(3, " to slot ");
      DebugPrintln(3, otherSlot);
      break;
    }
  }

  bool usedPriorities[4] = {false, false, false, false};
  for (int slot = 1; slot <= 3; slot++) {
    const int priority = getWifiPriorityFromConfig(slot);
    if (priority <= 0) {
      continue;
    }

    if (priority <= 3 && !usedPriorities[priority]) {
      usedPriorities[priority] = true;
      continue;
    }

    int replacementPriority = 0;
    for (int candidate = 1; candidate <= 3; candidate++) {
      if (!usedPriorities[candidate]) {
        replacementPriority = candidate;
        usedPriorities[candidate] = true;
        break;
      }
    }
    setWifiPriorityInConfig(slot, replacementPriority);
    DebugPrint(2, "Resolved duplicate WiFi priority for slot ");
    DebugPrintln(2, slot);
  }
}

unsigned int clampConfigUInt(unsigned int value, unsigned int fallback, unsigned int minValue, unsigned int maxValue) {
  if (value < minValue || value > maxValue) {
    return fallback;
  }

  return value;
}

String formatHexKey(const uint8_t *key, size_t length) {
  String value = "";
  for (size_t i = 0; i < length; i++) {
    String part = String(key[i], HEX);
    if (part.length() == 1) {
      part = "0" + part;
    }
    part.toUpperCase();
    value += part;
  }
  return value;
}

bool parseHexKey(const String &value, uint8_t *target, size_t targetLength) {
  if (value.length() != targetLength * 2) {
    return false;
  }

  char hexstring[3];
  hexstring[2] = '\0';
  for (size_t j = 0; j < targetLength; j++) {
    const char high = value[j * 2];
    const char low = value[j * 2 + 1];
    if (!isxdigit(high) || !isxdigit(low)) {
      return false;
    }
    hexstring[0] = high;
    hexstring[1] = low;
    target[j] = HexToInt(hexstring);
  }
  return true;
}

bool isPrintableAscii(const String &value) {
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (c < 32 || c > 126) {
      return false;
    }
  }
  return true;
}

bool isDefaultWebPasswordActive() {
  return isGeneratedWebPassword(actconf.password) || isLegacyWeakWebPassword(actconf.password);
}

bool requestHasTruthyBodyParam(AsyncWebServerRequest *request, const char *name) {
  if (request == nullptr || name == nullptr) {
    return false;
  }

  const AsyncWebParameter *param = request->getParam(name, true);
  if (param == nullptr) {
    return false;
  }

  String value = param->value();
  value.trim();
  value.toLowerCase();
  return value == "1" || value == "true" || value == "yes" || value == "on";
}

bool updateConfigStringField(char *target, size_t targetSize, const String &value) {
  if (target == nullptr || targetSize == 0) {
    return false;
  }

  value.toCharArray(target, targetSize);
  return true;
}

bool applyOptionalSecretValue(AsyncWebServerRequest *request,
                              const String &value,
                              const char *clearFlagName,
                              char *target,
                              size_t targetSize,
                              bool requirePrintableAscii = true) {
  if (requestHasTruthyBodyParam(request, clearFlagName)) {
    if (target != nullptr && targetSize > 0) {
      target[0] = '\0';
      return true;
    }
    return false;
  }

  if (value.length() == 0) {
    return false;
  }

  if (requirePrintableAscii && !isPrintableAscii(value)) {
    return false;
  }

  return updateConfigStringField(target, targetSize, value);
}

bool requireNonDefaultWebPassword(AsyncWebServerRequest *request) {
  if (!isDefaultWebPasswordActive()) {
    return true;
  }

  request->send(428, "application/json", "{\"status\":\"error\",\"message\":\"Please change the default web password before using this high-risk action.\"}");
  return false;
}

void noteWebActivity(unsigned long extendMs = 300000UL) {
  const unsigned long candidate = millis() + extendMs;
  if (candidate > standbySleepBlockedUntilMillis) {
    standbySleepBlockedUntilMillis = candidate;
  }
}
bool fetchHttpsText(const String &url, String &payload, String *errorMessage = nullptr, uint16_t timeoutMs = 10000) {
  payload = "";

  if (WiFi.status() != WL_CONNECTED) {
    if (errorMessage != nullptr) {
      *errorMessage = "No Wi-Fi connection available.";
    }
    return false;
  }

  const unsigned long waitStart = millis();
  while (!hasReasonableSystemTime() && millis() - waitStart < 4000UL) {
    delay(100);
  }

  if (!hasReasonableSystemTime()) {
    if (errorMessage != nullptr) {
      *errorMessage = "System time is not synchronized yet.";
    }
    return false;
  }

  String lastError = "HTTPS request failed";
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    WiFiClientSecure client;
    HTTPClient http;

    client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.addHeader("Cache-Control", "no-cache");
    http.addHeader("Pragma", "no-cache");

    if (!http.begin(client, url)) {
      lastError = "Unable to initialize HTTPS request";
    } else {
      const int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        payload.trim();
        http.end();
        if (payload.length() > 0) {
          return true;
        }
        lastError = "Empty response";
      } else if (httpCode > 0) {
        lastError = "HTTP " + String(httpCode);
      } else {
        lastError = http.errorToString(httpCode);
      }
      http.end();
    }

    if (attempt == 0) {
      delay(250);
    }
  }

  if (errorMessage != nullptr) {
    *errorMessage = lastError;
  }
  return false;
}

String extractVersionFromFirmwareUrl(const String &url) {
  const int firmwareIndex = url.lastIndexOf("/firmware.bin");
  if (firmwareIndex < 0) {
    return String();
  }

  const int versionEnd = firmwareIndex;
  const int versionStart = url.lastIndexOf('/', versionEnd - 1);
  if (versionStart < 0 || versionStart + 1 >= versionEnd) {
    return String();
  }

  return url.substring(versionStart + 1, versionEnd);
}

String normalizeVersionPayload(String payload) {
  payload.trim();
  if (payload.length() == 0) {
    return String();
  }

  if (payload.startsWith("{")) {
    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      const char *candidateKeys[] = {"version", "firmwareVersion", "latestVersion"};
      for (size_t i = 0; i < (sizeof(candidateKeys) / sizeof(candidateKeys[0])); ++i) {
        String value = String(doc[candidateKeys[i]] | "");
        value.trim();
        if (value.length() > 0) {
          return value;
        }
      }
    }
  }

  const int lineBreak = payload.indexOf('\n');
  if (lineBreak >= 0) {
    payload = payload.substring(0, lineBreak);
    payload.trim();
  }

  return payload;
}

uint16_t sampleBatteryAdcRaw(size_t samples = 8) {
  if (samples == 0) {
    samples = 1;
  }

  uint32_t total = 0;
  for (size_t i = 0; i < samples; ++i) {
    total += analogRead(ANALOG_IN);
    delay(2);
  }

  return uint16_t(total / samples);
}

bool fetchMdsOtaVersionMetadata(String &version, String *errorMessage = nullptr) {
  version = "";

  const String metadataUrl = buildMdsOtaMetadataUrl(String(actconf.mdsOtaUrl), "firmware.version");
  if (metadataUrl.length() == 0) {
    if (errorMessage != nullptr) {
      *errorMessage = "Could not derive MDS OTA metadata URL";
    }
    return false;
  }

  String payload;
  if (!fetchHttpsText(metadataUrl, payload, errorMessage)) {
    return false;
  }

  version = normalizeVersionPayload(payload);
  if (version.length() == 0 && errorMessage != nullptr) {
    *errorMessage = "Empty firmware version metadata";
  }
  return version.length() > 0;
}

String resolveFirmwareUrlFromManifestValue(const String &baseUrl, const String &manifestValue);

bool parseFirmwareReleasePayload(const String &payload,
                                 const String &channel,
                                 const String &baseUrl,
                                 String &firmwareUrl,
                                 String &version,
                                 String *errorMessage = nullptr,
                                 String *sha256 = nullptr,
                                 String *webFilesPath = nullptr) {
  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, payload);
  if (parseError) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest parse failed";
    }
    return false;
  }

  JsonObject release = doc[channel.c_str()].as<JsonObject>();
  if (release.isNull()) {
    release = doc.as<JsonObject>();
  }

  if (release.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest channel missing";
    }
    return false;
  }

  version = release["version"].as<String>();
  firmwareUrl = resolveFirmwareUrlFromManifestValue(baseUrl, release["firmware"].as<String>());
  if (sha256 != nullptr) {
    *sha256 = release["sha256"].as<String>();
    sha256->trim();
    sha256->toLowerCase();
  }
  if (webFilesPath != nullptr) {
    *webFilesPath = release["webFiles"].as<String>();
    webFilesPath->trim();
    while (webFilesPath->endsWith("/")) {
      webFilesPath->remove(webFilesPath->length() - 1);
    }
  }

  if (version.length() == 0 || firmwareUrl.length() == 0) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest release incomplete";
    }
    return false;
  }

  return true;
}

String getConfiguredFirmwareBaseUrl() {
  return buildMdsOtaWebBaseUrl(String(actconf.mdsOtaUrl));
}

String resolveFirmwareUrlFromManifestValue(const String &baseUrl, const String &manifestValue) {
  String value = manifestValue;
  value.trim();

  if (value.startsWith("https://")) {
    const String allowedPrefix = baseUrl + "/";
    return value.startsWith(allowedPrefix) ? value : String();
  }

  while (value.startsWith("/")) {
    value.remove(0, 1);
  }

  return baseUrl + "/" + value;
}

bool resolveFirmwareFromManifest(const String &channel, String &firmwareUrl, String &version, String *errorMessage, String *sha256, String *webFilesPath) {
  const String baseUrl = getConfiguredFirmwareBaseUrl();
  if (baseUrl.length() == 0) {
    if (errorMessage != nullptr) {
      *errorMessage = "MDS OTA endpoint is not configured";
    }
    return false;
  }

  const String candidates[] = {
    baseUrl + "/" + channel + ".json",
    baseUrl + "/firmware-manifest.json"
  };

  String payload;
  String lastError = "Unable to read OTA metadata.";
  for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
    payload = "";
    if (!fetchHttpsText(candidates[i], payload, &lastError)) {
      continue;
    }
    if (parseFirmwareReleasePayload(payload, channel, baseUrl, firmwareUrl, version, &lastError, sha256, webFilesPath)) {
      return true;
    }
  }

  if (errorMessage != nullptr) {
    *errorMessage = lastError;
  }
  return false;
}

void parseFirmwareVersion(const String &version, int &major, int &minor, String &suffix) {
  major = 0;
  minor = 0;
  suffix = "";

  String normalized = version;
  normalized.trim();
  if (normalized.startsWith("V") || normalized.startsWith("v")) {
    normalized.remove(0, 1);
  }

  const int dotIndex = normalized.indexOf('.');
  if (dotIndex < 0) {
    major = normalized.toInt();
    return;
  }

  major = normalized.substring(0, dotIndex).toInt();
  int suffixStart = dotIndex + 1;
  while (suffixStart < normalized.length() && isDigit(normalized.charAt(suffixStart))) {
    suffixStart++;
  }

  minor = normalized.substring(dotIndex + 1, suffixStart).toInt();
  suffix = normalized.substring(suffixStart);
  suffix.toLowerCase();
}

int compareVersionSuffix(const String &leftSuffix, const String &rightSuffix) {
  if (leftSuffix == rightSuffix) {
    return 0;
  }

  if (leftSuffix.length() == 0) {
    return -1;
  }

  if (rightSuffix.length() == 0) {
    return 1;
  }

  const int minLength = min(leftSuffix.length(), rightSuffix.length());
  for (int i = 0; i < minLength; i++) {
    const char leftChar = leftSuffix.charAt(i);
    const char rightChar = rightSuffix.charAt(i);
    if (leftChar != rightChar) {
      return leftChar > rightChar ? 1 : -1;
    }
  }

  return leftSuffix.length() > rightSuffix.length() ? 1 : -1;
}

int compareFirmwareVersions(const String &leftVersion, const String &rightVersion) {
  int leftMajor = 0;
  int leftMinor = 0;
  String leftSuffix;
  int rightMajor = 0;
  int rightMinor = 0;
  String rightSuffix;

  parseFirmwareVersion(leftVersion, leftMajor, leftMinor, leftSuffix);
  parseFirmwareVersion(rightVersion, rightMajor, rightMinor, rightSuffix);

  if (leftMajor != rightMajor) {
    return leftMajor > rightMajor ? 1 : -1;
  }

  if (leftMinor != rightMinor) {
    return leftMinor > rightMinor ? 1 : -1;
  }

  return compareVersionSuffix(leftSuffix, rightSuffix);
}

bool fetchMdsOtaInfo(const String &requestedChannel, JsonDocument &response) {
  const String channel = normalizeManifestChannel(requestedChannel);
  String normalizedMdsOtaEndpoint;
  const bool mdsOtaConfigured = normalizeMdsOtaEndpoint(String(actconf.mdsOtaUrl), normalizedMdsOtaEndpoint);
  response["configured"] = mdsOtaConfigured;
  response["installedVersion"] = String(actconf.fversion);
  response["installedChannel"] = getFirmwareReleaseChannel();
  response["installedChannelLabel"] = getFirmwareReleaseLabel();
  response["requestedChannel"] = channel;
  response["requestedChannelLabel"] = channelDisplayLabel(channel);
  response["version"] = "";
  response["status"] = "not-configured";
  response["message"] = "MDS OTA endpoint is not configured.";
  response["sha256Available"] = false;

  if (channel.length() == 0) {
    response["status"] = "error";
    response["message"] = "Unknown firmware channel requested.";
    return false;
  }

  if (!mdsOtaConfigured) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    response["status"] = "offline";
    response["message"] = "No Wi-Fi connection available.";
    return false;
  }

  String firmwareUrl;
  String version;
  String sha256;
  String webFilesPath;
  String resolveError;
  if (!resolveFirmwareFromManifest(channel, firmwareUrl, version, &resolveError, &sha256, &webFilesPath)) {
    response["status"] = "error";
    response["message"] = resolveError.length() ? resolveError : "Unable to read OTA manifest.";
    return false;
  }

  const String installedVersion = String(actconf.fversion);
  const String installedChannel = getFirmwareReleaseChannel();
  const bool sameVersion = version == installedVersion;
  const bool sameChannel = channel == installedChannel;

  response["version"] = version;
  response["firmwareUrl"] = firmwareUrl;
  response["webFilesPath"] = webFilesPath;
  response["sha256Available"] = sha256.length() == 64;

  if (sameVersion && sameChannel) {
    response["status"] = "current";
    response["message"] = channelDisplayLabel(channel) + " firmware is already installed.";
    return true;
  }

  response["status"] = "update-available";
  if (sameVersion && !sameChannel) {
    response["message"] = channelDisplayLabel(channel) + " build is available for the same version number.";
  } else {
    response["message"] = channelDisplayLabel(channel) + " firmware update available.";
  }
  return true;
}

void otaMetadataTask(void *parameter) {
  (void)parameter;
  const String channel = otaMetadataTaskChannel;
  JsonDocument response;
  fetchMdsOtaInfo(channel, response);
  String body;
  serializeJson(response, body);
  finishOtaMetadata(channel, body);
  releaseMaintenanceOperation(MaintenanceOperation::OtaMetadata);
  otaMetadataTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool queueOtaMetadata(const String &channel, String &errorMessage) {
  if (!acquireMaintenanceOperation(MaintenanceOperation::OtaMetadata)) {
    errorMessage = String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".";
    return false;
  }
  if (!beginOtaMetadata(channel)) {
    releaseMaintenanceOperation(MaintenanceOperation::OtaMetadata);
    errorMessage = "OTA metadata check is already running or the channel is invalid.";
    return false;
  }
  otaMetadataTaskChannel = channel;
  const BaseType_t created = xTaskCreate(otaMetadataTask, "otaMetadata", 12288, nullptr, 1, &otaMetadataTaskHandle);
  if (created != pdPASS) {
    otaMetadataTaskHandle = nullptr;
    finishOtaMetadata(channel, "{\"status\":\"error\",\"message\":\"Could not start OTA metadata task.\"}");
    releaseMaintenanceOperation(MaintenanceOperation::OtaMetadata);
    errorMessage = "Could not start OTA metadata task.";
    return false;
  }
  return true;
}

void addOtaDiagnosticCheck(JsonDocument &response,
                           const char *key,
                           const char *label,
                           bool ok,
                           const String &statusLabel,
                           const String &message) {
  JsonObject item = response[key].to<JsonObject>();
  item["label"] = label;
  item["ok"] = ok;
  item["statusLabel"] = statusLabel;
  item["message"] = message;
}

void buildOtaDiagnostics(JsonDocument &response) {
  response["installedVersion"] = String(actconf.fversion);
  response["installedChannel"] = getFirmwareReleaseChannel();
  response["installedChannelLabel"] = getFirmwareReleaseLabel();

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  addOtaDiagnosticCheck(response,
                        "wifi",
                        "Wi-Fi",
                        wifiConnected,
                        wifiConnected ? "OK" : "Offline",
                        wifiConnected ? ("Connected, IP " + WiFi.localIP().toString()) : "Wi-Fi is not connected.");

  const bool timeOk = hasReasonableSystemTime();
  addOtaDiagnosticCheck(response,
                        "time",
                        "Time",
                        timeOk,
                        timeOk ? "OK" : "Waiting",
                        timeOk ? ("Epoch " + String(time(nullptr))) : "System time is not synchronized yet.");

  const String baseUrl = getConfiguredFirmwareBaseUrl();
  const bool endpointOk = baseUrl.length() > 0;
  response["baseUrl"] = baseUrl;
  addOtaDiagnosticCheck(response,
                        "endpoint",
                        "Endpoint",
                        endpointOk,
                        endpointOk ? "OK" : "Missing",
                        endpointOk ? baseUrl : "MDS OTA endpoint is not configured or invalid.");

  if (!wifiConnected || !endpointOk) {
    addOtaDiagnosticCheck(response, "betaMetadata", "Beta Metadata", false, "Skipped", "Wi-Fi or endpoint is not ready.");
    addOtaDiagnosticCheck(response, "releaseMetadata", "Release Metadata", false, "Skipped", "Wi-Fi or endpoint is not ready.");
    addOtaDiagnosticCheck(response, "manifest", "Manifest", false, "Skipped", "Wi-Fi or endpoint is not ready.");
    return;
  }

  String payload;
  String errorMessage;
  const bool betaOk = fetchHttpsText(baseUrl + "/beta.json", payload, &errorMessage, 7000);
  addOtaDiagnosticCheck(response,
                        "betaMetadata",
                        "Beta Metadata",
                        betaOk,
                        betaOk ? "OK" : "Error",
                        betaOk ? ("beta.json OK, " + String(payload.length()) + " bytes") : errorMessage);

  payload = "";
  errorMessage = "";
  const bool stableOk = fetchHttpsText(baseUrl + "/stable.json", payload, &errorMessage, 7000);
  addOtaDiagnosticCheck(response,
                        "releaseMetadata",
                        "Release Metadata",
                        stableOk,
                        stableOk ? "OK" : "Fallback",
                        stableOk ? ("stable.json OK, " + String(payload.length()) + " bytes") : ("stable.json unavailable; manifest fallback will be used. " + errorMessage));

  payload = "";
  errorMessage = "";
  const bool manifestOk = fetchHttpsText(baseUrl + "/firmware-manifest.json", payload, &errorMessage, 9000);
  addOtaDiagnosticCheck(response,
                        "manifest",
                        "Manifest",
                        manifestOk,
                        manifestOk ? "OK" : "Error",
                        manifestOk ? ("firmware-manifest.json OK, " + String(payload.length()) + " bytes") : errorMessage);
}

void otaDiagnosticsTask(void *parameter) {
  (void)parameter;
  JsonDocument response;
  buildOtaDiagnostics(response);
  String body;
  serializeJson(response, body);
  finishOtaDiagnostics(body);
  releaseMaintenanceOperation(MaintenanceOperation::OtaDiagnostics);
  otaDiagnosticsTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool queueOtaDiagnostics(String &errorMessage) {
  if (!acquireMaintenanceOperation(MaintenanceOperation::OtaDiagnostics)) {
    errorMessage = String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".";
    return false;
  }
  if (!beginOtaDiagnostics()) {
    releaseMaintenanceOperation(MaintenanceOperation::OtaDiagnostics);
    errorMessage = "OTA diagnostics are already running.";
    return false;
  }
  const BaseType_t created = xTaskCreate(otaDiagnosticsTask, "otaDiagnostics", 12288, nullptr, 1, &otaDiagnosticsTaskHandle);
  if (created != pdPASS) {
    otaDiagnosticsTaskHandle = nullptr;
    finishOtaDiagnostics("{\"status\":\"error\",\"message\":\"Could not start OTA diagnostics task.\"}");
    releaseMaintenanceOperation(MaintenanceOperation::OtaDiagnostics);
    errorMessage = "Could not start OTA diagnostics task.";
    return false;
  }
  return true;
}

bool resolveStableFirmware(String &firmwareUrl, String &version, String *errorMessage = nullptr, String *sha256 = nullptr) {
  String manifestError;
  if (resolveFirmwareFromManifest("stable", firmwareUrl, version, &manifestError, sha256)) {
    return true;
  }

  const String baseUrl = getConfiguredFirmwareBaseUrl();
  const char *candidateFiles[] = {
    "latestVersion.txt",
    "latestStableVersion.txt",
    "latestFirmwareVersion.txt",
    "ActualVersion.txt"
  };

  String payload;
  String lastError;
  for (size_t i = 0; i < (sizeof(candidateFiles) / sizeof(candidateFiles[0])); ++i) {
    if (!fetchHttpsText(baseUrl + "/" + candidateFiles[i], payload, &lastError)) {
      continue;
    }

    if (payload.startsWith("https://")) {
      firmwareUrl = payload;
      version = extractVersionFromFirmwareUrl(payload);
      if (sha256 != nullptr) {
        *sha256 = "";
      }
      return firmwareUrl.length() > 0;
    }

    version = payload;
    firmwareUrl = baseUrl + "/" + version + "/firmware.bin";
    if (sha256 != nullptr) {
      *sha256 = "";
    }
    return version.length() > 0;
  }

  if (errorMessage != nullptr) {
    *errorMessage = lastError.length() ? lastError : (manifestError.length() ? manifestError : "No stable version marker found");
  }
  return false;
}

String settingsTemplateProcessor(const String &var) {
  if (var == "header") return getheader(actconf);
  if (var == "devname") return htmlEscape(String(actconf.devname));
  if (var == "cssid1") return htmlEscape(String(actconf.cssid1));
  if (var == "corder1") return String(actconf.corder1);
  if (var == "cpassword1") return htmlEscape(String(actconf.cpassword1));
  if (var == "cpassword1Masked") return maskSecret(String(actconf.cpassword1));
  if (var == "cssid2") return htmlEscape(String(actconf.cssid2));
  if (var == "corder2") return String(actconf.corder2);
  if (var == "cpassword2") return htmlEscape(String(actconf.cpassword2));
  if (var == "cpassword2Masked") return maskSecret(String(actconf.cpassword2));
  if (var == "cssid3") return htmlEscape(String(actconf.cssid3));
  if (var == "corder3") return String(actconf.corder3);
  if (var == "cpassword3") return htmlEscape(String(actconf.cpassword3));
  if (var == "cpassword3Masked") return maskSecret(String(actconf.cpassword3));
  if (var == "username") return htmlEscape(String(actconf.username));
  if (var == "password") return htmlEscape(String(actconf.password));
  if (var == "passwordMasked") return maskSecret(String(actconf.password));
  if (var == "SendDataViaWifi") return String(getindex(SendDataViaWifi, String(actconf.SendDataViaWifi)));
  if (var == "transmitPriority") return String(getindex(transmitPriority, String(actconf.transmitPriority)));
  if (var == "standbyAutoUpdate") return String(getindex(WifiStandbyMode, String(actconf.standbyAutoUpdate)));
  if (var == "standbyAutoUpdateIntervalHours") return String(actconf.standbyAutoUpdateIntervalHours);
  if (var == "mdsOtaUrl") return htmlEscape(String(actconf.mdsOtaUrl));
  if (var == "mdsOtaSecretMasked") return maskSecret(String(actconf.mdsOtaSecret));
  if (var == "csrfToken") return getCsrfToken();

  if (var == "MdsUrl") return htmlEscape(String(actconf.MdsUrl));
  if (var == "MdsApiKeyMasked") return maskSecret(String(actconf.MdsApiKey));
  if (var == "MdsSensorIdBattery") return String(actconf.MdsSensorIdBattery);
  if (var == "MdsSensorIdTanks") return String(actconf.MdsSensorIdTanks);
  if (var == "MdsSensorIdStatus") return String(actconf.MdsSensorIdStatus);
  if (var == "MdsSensorIdGps") return String(actconf.MdsSensorIdGps);
  if (var == "MdsSensorIdEnv") return String(actconf.MdsSensorIdEnv);
  if (var == "MdsSensorIdDewpoint") return String(actconf.MdsSensorIdDewpoint);
  if (var == "MdsSensorIdVedirect") return String(actconf.MdsSensorIdVedirect);

  if (var == "hostname") return htmlEscape(String(actconf.hostname));
  if (var == "sssid") return htmlEscape(String(actconf.sssid));
  if (var == "spassword") return htmlEscape(String(actconf.spassword));
  if (var == "spasswordMasked") return maskSecret(String(actconf.spassword));

  if (var == "crypt") return String(getindex(usepassword, String(actconf.crypt)));
  if (var == "instrumentSize") return String(getindex(isize, String(actconf.instrumentSize)));
  if (var == "timeout") return String(getindex(timeout, String(actconf.timeout)));
  if (var == "apchannel") return String(getindex(apchannel, String(actconf.apchannel)));
  if (var == "serverMode") return String(getindex(servermode, String(actconf.serverMode)));
  if (var == "mDNS") return String(getindex(mdnsservice, String(actconf.mDNS)));

  if (var == "lorafrequency") return String(getindex(lorafrequencys, String(actconf.lorafrequency)));
  if (var == "lchannel") return String(getindex(lchannel, String(actconf.lchannel)));
  if (var == "spreadf") return String(getindex(spreadf, String(actconf.spreadf)));
  if (var == "dynsf") return String(getindex(dynsf, String(actconf.dynsf)));
  if (var == "tinterval") return String(actconf.tinterval);
  if (var == "relay") return String(getindex(relay, String(actconf.relay)));

  if (var == "devaddr") {
    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    return mystring;
  }

  if (var == "nskey") {
    return "";
  }

  if (var == "nskeyMasked") {
    return maskSecret(formatHexKey(actconf.nskey, sizeof(actconf.nskey)));
  }

  if (var == "appkey") {
    return "";
  }

  if (var == "appkeyMasked") {
    return maskSecret(formatHexKey(actconf.appkey, sizeof(actconf.appkey)));
  }

  auto fmt5 = [](float value) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%.5f", value);
    return String(buf);
  };

  if (var == "a1t1slope") return fmt5(actconf.a1t1slope);
  if (var == "a2t1slope") return fmt5(actconf.a2t1slope);
  if (var == "t1offset") return fmt5(actconf.t1offset);
  if (var == "a2t2slope") return fmt5(actconf.a2t2slope);
  if (var == "a1t2slope") return fmt5(actconf.a1t2slope);
  if (var == "t2offset") return fmt5(actconf.t2offset);

  if (var == "voffset") return fmt5(actconf.voffset);
  if (var == "a2vslope") return fmt5(actconf.a2vslope);
  if (var == "a1vslope") return fmt5(actconf.a1vslope);

  if (var == "debug") return String(getindex(debugmode, String(actconf.debug)));
  if (var == "serspeed") return String(getindex(serspeed, String(actconf.serspeed)));
  if (var == "WebSerialDebug") return String(getindex(WebSerialDebug, String(actconf.WebSerialDebug)));
  if (var == "deviceID") return String(getindex(deviceid, String(actconf.deviceID)));
  if (var == "senddata") return String(getindex(senddata, String(actconf.senddata)));
  if (var == "vaverage") return String(getindex(vaverage, String(actconf.vaverage)));
  if (var == "t1average") return String(getindex(t1average, String(actconf.t1average)));
  if (var == "t2average") return String(getindex(t2average, String(actconf.t2average)));
  if (var == "tempSensorType") return String(getindex(tstype, String(actconf.tempSensorType)));
  if (var == "tempUnit") return String(getindex(tempunits, String(actconf.tempUnit)));
  if (var == "envSensor") return String(getindex(envSensor, String(actconf.envSensor)));
  if (var == "standbyMode") return String(getindex(standbyMode, String(actconf.standbyMode)));
  if (var == "standbySleepDuration") return String(actconf.standbySleepDuration);
  if (var == "loraOperationMode") return String(getindex(loraOperationMode, String(actconf.loraOperationMode)));
  if (var == "WifiStandbyMode") return String(getindex(WifiStandbyMode, String(actconf.WifiStandbyMode)));
  if (var == "cssStyle") return String(getindex(cssStyle, String(actconf.cssStyle)));
  if (var == "OledDisplayRotation") return String(getindex(OledDisplayRotation, String(actconf.OledDisplayRotation)));
  if (var == "OledDisplayMode") return String(getindex(OledDisplayMode, String(actconf.OledDisplayMode)));

  return String();
}

String buildOtaResponse(const char *status, const String &message, bool rebooting, bool checkWebFiles, bool backupSaved) {
  String response = "{\"status\":\"";
  response += status;
  response += "\",\"message\":\"";
  response += message;
  response += "\",\"rebooting\":";
  response += rebooting ? "true" : "false";
  response += ",\"checkWebFiles\":";
  response += checkWebFiles ? "true" : "false";
  response += ",\"backupSaved\":";
  response += backupSaved ? "true" : "false";
  response += "}";
  return response;
}

bool applySettingsField(AsyncWebServerRequest *request, const String &fieldName, const String &fieldValue) {
  if (fieldName == "usepassword") {
    actconf.crypt = 1;
    return true;
  }
  if (fieldName == "username") {
    updateConfigStringField(actconf.username, sizeof(actconf.username), fieldValue);
    return true;
  }
  if (fieldName == "pagepasswd") {
    if (fieldValue.length() > 0) {
      updateConfigStringField(actconf.password, sizeof(actconf.password), fieldValue);
    }
    return true;
  }
  if (fieldName == "isize") {
    actconf.instrumentSize = toInteger(fieldValue);
    return true;
  }
  if (fieldName == "hostname") {
    updateConfigStringField(actconf.hostname, sizeof(actconf.hostname), fieldValue);
    return true;
  }
  if (fieldName == "cssid1") {
    updateConfigStringField(actconf.cssid1, sizeof(actconf.cssid1), fieldValue);
    return true;
  }
  if (fieldName == "corder1") {
    actconf.corder1 = clampConfigInt(toInteger(fieldValue), defconf.corder1, 0, 3);
    return true;
  }
  if (fieldName == "cpasswd1") {
    applyOptionalSecretValue(request, fieldValue, "clearCPassword1", actconf.cpassword1, sizeof(actconf.cpassword1));
    return true;
  }
  if (fieldName == "cssid2") {
    updateConfigStringField(actconf.cssid2, sizeof(actconf.cssid2), fieldValue);
    return true;
  }
  if (fieldName == "corder2") {
    actconf.corder2 = clampConfigInt(toInteger(fieldValue), defconf.corder2, 0, 3);
    return true;
  }
  if (fieldName == "cpasswd2") {
    applyOptionalSecretValue(request, fieldValue, "clearCPassword2", actconf.cpassword2, sizeof(actconf.cpassword2));
    return true;
  }
  if (fieldName == "cssid3") {
    updateConfigStringField(actconf.cssid3, sizeof(actconf.cssid3), fieldValue);
    return true;
  }
  if (fieldName == "corder3") {
    actconf.corder3 = clampConfigInt(toInteger(fieldValue), defconf.corder3, 0, 3);
    return true;
  }
  if (fieldName == "cpasswd3") {
    applyOptionalSecretValue(request, fieldValue, "clearCPassword3", actconf.cpassword3, sizeof(actconf.cpassword3));
    return true;
  }
  if (fieldName == "timeout") {
    actconf.timeout = clampConfigInt(toInteger(fieldValue), defconf.timeout, 3, 240);
    return true;
  }
  if (fieldName == "sssid") {
    updateConfigStringField(actconf.sssid, sizeof(actconf.sssid), fieldValue);
    return true;
  }
  if (fieldName == "spasswd") {
    if (fieldValue.length() > 0) {
      updateConfigStringField(actconf.spassword, sizeof(actconf.spassword), fieldValue);
    }
    return true;
  }
  if (fieldName == "apchannel") {
    actconf.apchannel = clampConfigInt(toInteger(fieldValue), defconf.apchannel, 1, 13);
    return true;
  }
  if (fieldName == "mdsOtaUrl") {
    String normalizedMdsOtaUrl;
    if (normalizeMdsOtaEndpoint(fieldValue, normalizedMdsOtaUrl)) {
      normalizedMdsOtaUrl.toCharArray(actconf.mdsOtaUrl, sizeof(actconf.mdsOtaUrl));
    } else if (fieldValue.length() > 0) {
      DebugPrintln(2, "Ignoring invalid MDS OTA URL while saving settings");
    }
    return true;
  }
  if (fieldName == "mdsOtaSecret") {
    applyOptionalSecretValue(request, fieldValue, "clearMdsOtaSecret", actconf.mdsOtaSecret, sizeof(actconf.mdsOtaSecret));
    return true;
  }
  if (fieldName == "servermode") {
    actconf.serverMode = clampConfigInt(toInteger(fieldValue), defconf.serverMode, 0, 4);
    return true;
  }
  if (fieldName == "mdnsservice") {
    actconf.mDNS = clampConfigInt(toInteger(fieldValue), defconf.mDNS, 0, 1);
    return true;
  }
  if (fieldName == "SendDataViaWifi") {
    updateConfigStringField(actconf.SendDataViaWifi, sizeof(actconf.SendDataViaWifi), fieldValue);
    return true;
  }
  if (fieldName == "transmitPriority") {
    updateConfigStringField(actconf.transmitPriority, sizeof(actconf.transmitPriority), fieldValue);
    return true;
  }
  if (fieldName == "MdsUrl") {
    String mdsUrl = fieldValue;
    mdsUrl.trim();
    if (mdsUrl.length() > 0 && mdsUrl.indexOf("://") < 0) {
      mdsUrl = "https://" + mdsUrl;
    }
    mdsUrl.replace("https://s-git.derguntmar.de/", "https://mds-git.derguntmar.de/");
    mdsUrl.replace("https://git.derguntmar.de/", "https://mds-git.derguntmar.de/");
    if (mdsUrl == "https://mds-git.derguntmar.de" || mdsUrl == "https://mds-git.derguntmar.de/") {
      mdsUrl = String(defconf.MdsUrl);
    }
    if (mdsUrl.startsWith("https://") && mdsUrl.length() < sizeof(actconf.MdsUrl)) {
      mdsUrl.toCharArray(actconf.MdsUrl, sizeof(actconf.MdsUrl));
    }
    return true;
  }
  if (fieldName == "MdsApiKey") {
    applyOptionalSecretValue(request, fieldValue, "clearMdsApiKey", actconf.MdsApiKey, sizeof(actconf.MdsApiKey));
    return true;
  }
  if (fieldName == "MdsSensorIdBattery") {
    actconf.MdsSensorIdBattery = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdBattery, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdTanks") {
    actconf.MdsSensorIdTanks = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdTanks, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdStatus") {
    actconf.MdsSensorIdStatus = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdStatus, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdGps") {
    actconf.MdsSensorIdGps = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdGps, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdEnv") {
    actconf.MdsSensorIdEnv = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdEnv, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdDewpoint") {
    actconf.MdsSensorIdDewpoint = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdDewpoint, 0, 1);
    return true;
  }
  if (fieldName == "MdsSensorIdVedirect") {
    actconf.MdsSensorIdVedirect = clampConfigInt(toInteger(fieldValue), defconf.MdsSensorIdVedirect, 0, 1);
    return true;
  }
  if (fieldName == "devaddr") {
    char hexstring[9];
    fieldValue.toCharArray(hexstring, sizeof(hexstring));
    actconf.devaddr = HexToInt(hexstring);
    return true;
  }
  if (fieldName == "nskey") {
    if (fieldValue.length() > 0) {
      if (parseHexKey(fieldValue, actconf.nskey, sizeof(actconf.nskey))) {
        DebugPrintln(3, "LoRa network session key updated");
      } else {
        DebugPrintln(2, "Invalid LoRa network session key ignored");
      }
    }
    return true;
  }
  if (fieldName == "appkey") {
    if (fieldValue.length() > 0) {
      if (parseHexKey(fieldValue, actconf.appkey, sizeof(actconf.appkey))) {
        DebugPrintln(3, "LoRa application session key updated");
      } else {
        DebugPrintln(2, "Invalid LoRa application session key ignored");
      }
    }
    return true;
  }
  if (fieldName == "lorafrequency") {
    updateConfigStringField(actconf.lorafrequency, sizeof(actconf.lorafrequency), fieldValue);
    return true;
  }
  if (fieldName == "lchannel") {
    actconf.lchannel = clampConfigInt(toInteger(fieldValue), defconf.lchannel, 0, 9);
    return true;
  }
  if (fieldName == "dynsf") {
    actconf.dynsf = clampConfigInt(toInteger(fieldValue), defconf.dynsf, 0, 1);
    return true;
  }
  if (fieldName == "spreadf") {
    actconf.spreadf = clampConfigInt(toInteger(fieldValue), defconf.spreadf, 7, 10);
    return true;
  }
  if (fieldName == "tinterval") {
    const unsigned int newInterval = clampConfigUInt(toInteger(fieldValue), defconf.tinterval, 1U, 255U);
    if (actconf.tinterval != newInterval) {
      actconf.tinterval = newInterval;
      TX_INTERVAL = actconf.tinterval * 60;
    }
    return true;
  }
  if (fieldName == "relay") {
    actconf.relay = clampConfigInt(toInteger(fieldValue), defconf.relay, 0, 2);
    if (actconf.relay == 0) {
      digitalWrite(relayPin, LOW);
      relaytimer = 0;
    } else {
      digitalWrite(relayPin, HIGH);
    }
    return true;
  }
  if (fieldName == "debugmode") {
    actconf.debug = clampConfigInt(toInteger(fieldValue), defconf.debug, 0, 3);
    return true;
  }
  if (fieldName == "serspeed") {
    actconf.serspeed = clampConfigInt(toInteger(fieldValue), defconf.serspeed, 300, 115200);
    return true;
  }
  if (fieldName == "WebSerialDebug") {
    actconf.WebSerialDebug = clampConfigInt(toInteger(fieldValue), defconf.WebSerialDebug, 0, 1);
    return true;
  }
  if (fieldName == "deviceid") {
    actconf.deviceID = clampConfigInt(toInteger(fieldValue), defconf.deviceID, 0, 9);
    return true;
  }
  if (fieldName == "senddata") {
    actconf.senddata = clampConfigInt(toInteger(fieldValue), defconf.senddata, 0, 1);
    return true;
  }
  if (fieldName == "vaverage") {
    actconf.vaverage = clampConfigInt(toInteger(fieldValue), defconf.vaverage, 1, 100);
    return true;
  }
  if (fieldName == "t1average") {
    actconf.t1average = clampConfigInt(toInteger(fieldValue), defconf.t1average, 1, 100);
    return true;
  }
  if (fieldName == "t2average") {
    actconf.t2average = clampConfigInt(toInteger(fieldValue), defconf.t2average, 1, 100);
    return true;
  }
  if (fieldName == "tstype") {
    updateConfigStringField(actconf.tempSensorType, sizeof(actconf.tempSensorType), fieldValue);
    return true;
  }
  if (fieldName == "tempunit") {
    updateConfigStringField(actconf.tempUnit, sizeof(actconf.tempUnit), fieldValue);
    return true;
  }
  if (fieldName == "envSensor") {
    updateConfigStringField(actconf.envSensor, sizeof(actconf.envSensor), fieldValue);
    return true;
  }
  if (fieldName == "standbyMode") {
    updateConfigStringField(actconf.standbyMode, sizeof(actconf.standbyMode), fieldValue);
    return true;
  }
  if (fieldName == "standbySleepDuration") {
    actconf.standbySleepDuration = clampConfigInt(toInteger(fieldValue), defconf.standbySleepDuration, 1, 1440);
    return true;
  }
  if (fieldName == "loraOperationMode") {
    updateConfigStringField(actconf.loraOperationMode, sizeof(actconf.loraOperationMode), fieldValue);
    return true;
  }
  if (fieldName == "WifiStandbyMode") {
    updateConfigStringField(actconf.WifiStandbyMode, sizeof(actconf.WifiStandbyMode), fieldValue);
    return true;
  }
  if (fieldName == "standbyAutoUpdate") {
    updateConfigStringField(actconf.standbyAutoUpdate, sizeof(actconf.standbyAutoUpdate), fieldValue);
    return true;
  }
  if (fieldName == "standbyAutoUpdateIntervalHours") {
    actconf.standbyAutoUpdateIntervalHours = clampConfigInt(toInteger(fieldValue), defconf.standbyAutoUpdateIntervalHours, 1, 720);
    return true;
  }
  if (fieldName == "a1vslope") {
    actconf.a1vslope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "a2vslope") {
    actconf.a2vslope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "voffset") {
    actconf.voffset = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "a1t1slope") {
    actconf.a1t1slope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "a2t1slope") {
    actconf.a2t1slope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "t1offset") {
    actconf.t1offset = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "a1t2slope") {
    actconf.a1t2slope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "a2t2slope") {
    actconf.a2t2slope = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "t2offset") {
    actconf.t2offset = toFloat(fieldValue);
    return true;
  }
  if (fieldName == "cssStyle") {
    actconf.cssStyle = clampConfigInt(toInteger(fieldValue), defconf.cssStyle, 0, 2);
    return true;
  }
  if (fieldName == "OledDisplayRotation") {
    actconf.OledDisplayRotation = clampConfigInt(toInteger(fieldValue), defconf.OledDisplayRotation, 0, 1);
    return true;
  }
  if (fieldName == "OledDisplayMode") {
    updateConfigStringField(actconf.OledDisplayMode, sizeof(actconf.OledDisplayMode), fieldValue);
    return true;
  }

  return false;
}

String buildInitialSetupTemplate(const String &baseContent) {
  String content = baseContent;
  content.replace("%devname%", htmlEscape(String(actconf.devname)));
  content.replace("%crights%", htmlEscape(String(actconf.crights)));
  content.replace("%fversion%", String(actconf.fversion));
  content.replace("%cssid1%", htmlEscape(String(actconf.cssid1)));
  content.replace("%cpassword1%", "");
  content.replace("%cpassword1Masked%", maskSecret(String(actconf.cpassword1)));
  content.replace("%cssid2%", htmlEscape(String(actconf.cssid2)));
  content.replace("%cpassword2%", "");
  content.replace("%cpassword2Masked%", maskSecret(String(actconf.cpassword2)));
  content.replace("%cssid3%", htmlEscape(String(actconf.cssid3)));
  content.replace("%cpassword3%", "");
  content.replace("%cpassword3Masked%", maskSecret(String(actconf.cpassword3)));
  content.replace("%quality%", String(int(quality)));
  content.replace("%csrfToken%", getCsrfToken());
  content.replace("%FREE_FILESYSTEM%", humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes())));
  content.replace("%USED_FILESYSTEM%", humanReadableSize(LittleFS.usedBytes()));
  content.replace("%TOTAL_FILESYSTEM%", humanReadableSize(LittleFS.totalBytes()));
  content.replace("%USED_FILESYSTEM_BYTES%", String(LittleFS.usedBytes()));
  content.replace("%TOTAL_FILESYSTEM_BYTES%", String(LittleFS.totalBytes()));
  return content;
}

String buildEnvironmentSensorMarkup() {
  if (String(actconf.envSensor) != "BME280") {
    return "";
  }

  String envSensorString = "";
  envSensorString += F("<h3>Environment <blink><data id='info'></data></blink>");
  envSensorString += F("</h3>");
  envSensorString += F("<FONT SIZE='4'>");
  envSensorString += F("<table>");
  envSensorString += F("<tr>");
  envSensorString += F("<td>");
  envSensorString += F("<div class='svg'>");
  envSensorString += F("<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
  envSensorString += F("<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
  envSensorString += F("<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
  envSensorString += F("<line x1='10' y1='9' x2='14' y2='9' />");
  envSensorString += F("</svg>");
  envSensorString += F("</div>");
  envSensorString += F("</td>");
  envSensorString += F("<td>Temp:</td>");
  envSensorString += F("<td><data id='airtemp'></data><data id='atunit'></data></td>");
  envSensorString += F("<td></td>");
  envSensorString += F("</tr>");
  envSensorString += F("<tr>");
  envSensorString += F("<td>");
  envSensorString += F("<div class='svg'>");
  envSensorString += F("<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-wind' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
  envSensorString += F("<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
  envSensorString += F("<path d='M5 8h8.5a2.5 2.5 0 1 0 -2.34 -3.24' />");
  envSensorString += F("<path d='M3 12h15.5a2.5 2.5 0 1 1 -2.34 3.24' />");
  envSensorString += F("<path d='M4 16h5.5a2.5 2.5 0 1 1 -2.34 3.24' />");
  envSensorString += F("</svg>");
  envSensorString += F("</div>");
  envSensorString += F("</td>");
  envSensorString += F("<td>Press:</td>");
  envSensorString += F("<td><data id='pressure'></data>mbar</td>");
  envSensorString += F("<td></td>");
  envSensorString += F("</tr>");
  envSensorString += F("<tr>");
  envSensorString += F("<td>");
  envSensorString += F("<div class='svg'>");
  envSensorString += F("<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-droplet' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
  envSensorString += F("<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
  envSensorString += F("<path d='M6.8 11a6 6 0 1 0 10.396 0l-5.197 -8l-5.2 8z' />");
  envSensorString += F("</svg>");
  envSensorString += F("</div>");
  envSensorString += F("</td>");
  envSensorString += F("<td>Hum:</td>");
  envSensorString += F("<td><data id='humidity'></data>%</td>");
  envSensorString += F("<td></td>");
  envSensorString += F("</tr>");
  envSensorString += F("<tr>");
  envSensorString += F("<td>");
  envSensorString += F("<div class='svg'>");
  envSensorString += F("<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
  envSensorString += F("<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
  envSensorString += F("<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
  envSensorString += F("<line x1='10' y1='9' x2='14' y2='9' />");
  envSensorString += F("</svg>");
  envSensorString += F("</div>");
  envSensorString += F("</td>");
  envSensorString += F("<td>Dew:</td>");
  envSensorString += F("<td><data id='dewpoint'></data><data id='dunit'></data></td>");
  envSensorString += F("<td></td>");
  envSensorString += F("</tr>");
  envSensorString += F("</table>");
  envSensorString += F("</FONT><br>");
  envSensorString += F("<hr align='left'>");
  return envSensorString;
}
}  // namespace

void cleanupStaleWebBundleArtifacts() {
  cleanupStaleWebBundleArtifactsInternal();
}

void noteWebApiActivity() {
  noteWebActivity();
}

void notePageRequestActivity() {
  noteWebActivity();
}

bool requireStrongPagePassword(AsyncWebServerRequest *request) {
  return requireNonDefaultWebPassword(request);
}

String escapePageHtml(const String &value) {
  return htmlEscape(value);
}

String renderInitialSetupPage(const String &baseContent) {
  return buildInitialSetupTemplate(baseContent);
}

String renderEnvironmentSensorMarkup() {
  return buildEnvironmentSensorMarkup();
}

String processSettingsPageTemplate(const String &variable) {
  return settingsTemplateProcessor(variable);
}

uint16_t samplePageBatteryAdcRaw(size_t samples) {
  return sampleBatteryAdcRaw(samples);
}

String createPageOtaResponse(const char *status, const String &message, bool rebooting, bool checkWebFiles, bool backupSaved) {
  return buildOtaResponse(status, message, rebooting, checkWebFiles, backupSaved);
}

bool queuePageRemoteOtaUpdate(const String &source, bool forceReinstall, String &errorMessage) {
  return queueRemoteOtaUpdateFromMdsEndpoint(source, forceReinstall, errorMessage);
}

bool installWebBundleFromTar(const String &bundlePath, String &installedVersion, String &errorMessage) {
  return installWebBundleFromTarInternal(bundlePath, installedVersion, errorMessage);
}

bool resolveOtaFirmwareForChannel(const String &channel, String &firmwareUrl, String &version, String *errorMessage, String *sha256, String *webFilesPath) {
  return resolveFirmwareFromManifest(channel, firmwareUrl, version, errorMessage, sha256, webFilesPath);
}

int compareOtaFirmwareVersions(const String &leftVersion, const String &rightVersion) {
  return compareFirmwareVersions(leftVersion, rightVersion);
}

//const int relayPin = 25;      // Pin GPIO25, Relay is high activ

bool requireAuthenticatedRequest(AsyncWebServerRequest *request) {
  if (actconf.crypt != 1) {
    return true;
  }

  if (request->authenticate(actconf.username, actconf.password)) {
    return true;
  }

  request->requestAuthentication();
  return false;
}

bool requirePageRequestAccess(AsyncWebServerRequest *request) {
  if (!requireAuthenticatedRequest(request)) {
    return false;
  }

  noteWebActivity();
  return true;
}

bool requirePostRequestAccess(AsyncWebServerRequest *request, bool requireStrongPassword = false) {
  if (!requireAuthenticatedRequest(request)) {
    return false;
  }
  if (!requireCsrfToken(request)) {
    return false;
  }
  if (requireStrongPassword && !requireNonDefaultWebPassword(request)) {
    return false;
  }
  return true;
}

void handleSaveSettingsRoute(AsyncWebServerRequest *request) {
  if (!requireAuthenticatedRequest(request)) {
    return;
  }
  if (!requireCsrfToken(request)) {
    DebugPrintln(2, "Blocked saving settings without a valid CSRF token.");
    return;
  }
  const MaintenanceOperation maintenance = getMaintenanceOperation();
  if (maintenance != MaintenanceOperation::None) {
    request->send(409, "application/json", String("{\"status\":\"error\",\"message\":\"Settings cannot be changed while ") + maintenanceOperationName(maintenance) + " is running.\"}");
    return;
  }

  const configData previousConfig = actconf;

  const WifiPrioritySnapshot previousWifiPriorities = {
    actconf.corder1,
    actconf.corder2,
    actconf.corder3
  };

  int num = request->args();
  for (int i = 0; i < num; i++) {
    const String fieldName = request->argName(i);
    const String fieldValue = request->arg(i);
    applySettingsField(request, fieldName, fieldValue);
  }

  ensureUniqueWifiPriorities(previousWifiPriorities);

  if (sanitizeBatteryCalibration(actconf, defconf)) {
    DebugPrintln(2, "Battery calibration reset to safe defaults while saving settings");
  }

  if (num > 0) {
    if (!saveEEPROMConfig(actconf)) {
      actconf = previousConfig;
      request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Settings could not be stored in EEPROM.\"}");
      return;
    }
    standbySleepBlockedUntilMillis = millis() + 30000UL;
    DebugPrintln(3, "New settings saved");
  }
  DebugPrintln(3, "Send settings.html");
  request->redirect("/settings.html");
}

void handleSettingsPageSave(AsyncWebServerRequest *request) {
  handleSaveSettingsRoute(request);
}


void registerMaintenanceRoutes() {
  httpServer.on("/reseteeprom", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    actconf = defconf;
    saveEEPROMConfig(defconf);
    request->send(200, "text/javascript", "ok, EEPROM erased.");
    scheduledRestartMillis = millis() + 1500UL;
  });

  httpServer.on("/restoreconfigbackup", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }

    configData restoredConfig;
    if (!restoreConfigBackupFromLittleFS(restoredConfig)) {
      request->send(404, "application/json", buildOtaResponse("error", "No valid config backup found in LittleFS.", false, false, false));
      return;
    }

    String currentVersion = String(defconf.fversion);
    currentVersion.toCharArray(restoredConfig.fversion, sizeof(restoredConfig.fversion));
    restoredConfig.valid = defconf.valid;
    if (restoredConfig.password[0] == '\0') {
      strncpy(restoredConfig.password, actconf.password, sizeof(restoredConfig.password) - 1);
      restoredConfig.password[sizeof(restoredConfig.password) - 1] = '\0';
    }
    if (restoredConfig.cpassword1[0] == '\0') {
      strncpy(restoredConfig.cpassword1, actconf.cpassword1, sizeof(restoredConfig.cpassword1) - 1);
      restoredConfig.cpassword1[sizeof(restoredConfig.cpassword1) - 1] = '\0';
    }
    if (restoredConfig.cpassword2[0] == '\0') {
      strncpy(restoredConfig.cpassword2, actconf.cpassword2, sizeof(restoredConfig.cpassword2) - 1);
      restoredConfig.cpassword2[sizeof(restoredConfig.cpassword2) - 1] = '\0';
    }
    if (restoredConfig.cpassword3[0] == '\0') {
      strncpy(restoredConfig.cpassword3, actconf.cpassword3, sizeof(restoredConfig.cpassword3) - 1);
      restoredConfig.cpassword3[sizeof(restoredConfig.cpassword3) - 1] = '\0';
    }
    if (restoredConfig.spassword[0] == '\0') {
      strncpy(restoredConfig.spassword, actconf.spassword, sizeof(restoredConfig.spassword) - 1);
      restoredConfig.spassword[sizeof(restoredConfig.spassword) - 1] = '\0';
    }
    if (restoredConfig.MdsApiKey[0] == '\0') {
      strncpy(restoredConfig.MdsApiKey, actconf.MdsApiKey, sizeof(restoredConfig.MdsApiKey) - 1);
      restoredConfig.MdsApiKey[sizeof(restoredConfig.MdsApiKey) - 1] = '\0';
    }
    if (restoredConfig.mdsOtaSecret[0] == '\0') {
      strncpy(restoredConfig.mdsOtaSecret, actconf.mdsOtaSecret, sizeof(restoredConfig.mdsOtaSecret) - 1);
      restoredConfig.mdsOtaSecret[sizeof(restoredConfig.mdsOtaSecret) - 1] = '\0';
    }
    bool hasRestoredNskey = false;
    bool hasRestoredAppkey = false;
    for (size_t i = 0; i < sizeof(restoredConfig.nskey); i++) {
      hasRestoredNskey = hasRestoredNskey || restoredConfig.nskey[i] != 0;
      hasRestoredAppkey = hasRestoredAppkey || restoredConfig.appkey[i] != 0;
    }
    if (!hasRestoredNskey) {
      memcpy(restoredConfig.nskey, actconf.nskey, sizeof(restoredConfig.nskey));
    }
    if (!hasRestoredAppkey) {
      memcpy(restoredConfig.appkey, actconf.appkey, sizeof(restoredConfig.appkey));
    }
    actconf = restoredConfig;
    saveEEPROMConfig(actconf);

    request->send(200, "application/json", buildOtaResponse("ok", "Config backup restored. Device reboots now.", true, false, true));
    scheduledRestartMillis = millis() + 1500UL;
  });

  httpServer.on("/gauge.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsTextFile(request, "/gauge.min.js", "text/javascript");
  });

  auto handleOtaPost = [](AsyncWebServerRequest *request) {
      if (actconf.crypt == 1) {
        if(!request->authenticate(actconf.username, actconf.password)) {
          return request->requestAuthentication();
        }
      }
      if (!requireCsrfToken(request)) {
        return;
      }
      if (!requireNonDefaultWebPassword(request)) {
        return;
      }
    };

  auto handleOtaUpload = [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {
                    if (actconf.crypt == 1) {
                      if(!request->authenticate(actconf.username, actconf.password)) {
                        return;
                      }
                    }
                    if (!isCsrfTokenValid(request)) {
                      if (index == 0) {
                        finishOtaProgress(false, "Invalid CSRF token.");
                      }
                      return;
                    }
                    if (isDefaultWebPasswordActive()) {
                      if (index == 0) {
                        finishOtaProgress(false, "Default web password must be changed before firmware updates.");
                      }
                      return;
                    }
                    handleDoUpdate(request, filename, index, data, len, final);
                  };

  httpServer.on("/doUpdate", HTTP_POST,
    handleOtaPost,
    handleOtaUpload
  );

  #ifdef ESP32
    //Update.onProgress(printProgress);
  #endif

  // run handleUpload function when any file is uploaded
  httpServer.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (actconf.crypt == 1) {
        if(!request->authenticate(actconf.username, actconf.password)) {
          return request->requestAuthentication();
        }
      }
      if (!requireCsrfToken(request)) {
        return;
      }
      if (!requireNonDefaultWebPassword(request)) {
        return;
      }
      if (!singleFileUploadFinished) {
        request->send(500, "text/plain", "File upload did not complete.");
        return;
      }
      if (!singleFileUploadSucceeded) {
        request->send(400, "text/plain", singleFileUploadMessage.length() ? singleFileUploadMessage : "File upload failed.");
        return;
      }
      request->redirect("/filesystem.html?upload=success");
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {
                    if (actconf.crypt == 1) {
                      if(!request->authenticate(actconf.username, actconf.password)) {
                        return;
                      }
                    }
                    if (!isCsrfTokenValid(request)) {
                      return;
                    }
                    if (!index) {
                      singleFileUploadFinished = false;
                      singleFileUploadSucceeded = false;
                      singleFileUploadMessage = "File upload failed.";
                      if (isDefaultWebPasswordActive()) {
                        singleFileUploadFinished = true;
                        singleFileUploadMessage = "Default web password must be changed before file uploads.";
                        return;
                      }
                      if (!acquireMaintenanceOperation(MaintenanceOperation::SingleFileUpload)) {
                        singleFileUploadFinished = true;
                        singleFileUploadMessage = String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".";
                        return;
                      }
                    }
                    if (singleFileUploadFinished && !singleFileUploadSucceeded) {
                      return;
                    }
                    const bool uploadOk = handleUpload(request, filename, index, data, len, final);
                    if (!uploadOk) {
                      singleFileUploadFinished = true;
                      singleFileUploadMessage = "File upload was rejected or could not be written.";
                      releaseMaintenanceOperation(MaintenanceOperation::SingleFileUpload);
                      return;
                    }
                    if (final) {
                      singleFileUploadFinished = true;
                      singleFileUploadSucceeded = true;
                      singleFileUploadMessage = "File uploaded successfully.";
                      releaseMaintenanceOperation(MaintenanceOperation::SingleFileUpload);
                    }
                  }
  );

  httpServer.on("/uploadWebBundle", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (actconf.crypt == 1) {
        if(!request->authenticate(actconf.username, actconf.password)) {
          return request->requestAuthentication();
        }
      }
      if (!requireCsrfToken(request)) {
        return;
      }
      if (!webBundleUploadFinished) {
        setWebBundleUploadResult(500, "error", "Web package upload did not complete.", false);
      }
      const int responseStatusCode = webBundleUploadStatusCode;
      const String responseBody = webBundleUploadResponse;
      resetWebBundleUploadState();
      request->send(responseStatusCode, "application/json", responseBody);
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {
                    if (actconf.crypt == 1) {
                      if(!request->authenticate(actconf.username, actconf.password)) {
                        return;
                      }
                    }
                    if (!isCsrfTokenValid(request)) {
                      return;
                    }
                    if (!index) {
                      resetWebBundleUploadState();
                      if (!acquireMaintenanceOperation(MaintenanceOperation::WebBundle)) {
                        setWebBundleUploadResult(409, "error", String("Device maintenance is busy with ") + maintenanceOperationName(getMaintenanceOperation()) + ".", false);
                        return;
                      }
                    }
                    if (webBundleUploadFailed) {
                      return;
                    }

                    String loweredFilename = filename;
                    loweredFilename.toLowerCase();
                    if (filename.length() == 0 || !loweredFilename.endsWith(".tar")) {
                      setWebBundleUploadResult(400, "error", "Please upload a .tar web package.", false);
                      releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                      return;
                    }

                    if (index + len > MAX_WEB_BUNDLE_UPLOAD_SIZE) {
                      if (request->_tempFile) {
                        request->_tempFile.close();
                      }
                      LittleFS.remove(WEB_BUNDLE_UPLOAD_PATH);
                      setWebBundleUploadResult(413, "error", "Web package is too large.", false);
                      releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                      return;
                    }

                    if (!index) {
                      localOtaInProgress = true;
                      localOtaUpdateWriterActive = false;
                      localOtaLastActivityMillis = millis();
                      standbySleepBlockedUntilMillis = millis() + 180000UL;
                      startOtaProgress("upload-web-package", request->contentLength(), "Uploading web package...");
                      if (LittleFS.exists(WEB_BUNDLE_UPLOAD_PATH)) {
                        LittleFS.remove(WEB_BUNDLE_UPLOAD_PATH);
                      }
                      request->_tempFile = LittleFS.open(WEB_BUNDLE_UPLOAD_PATH, FILE_WRITE);
                      if (!request->_tempFile) {
                        localOtaInProgress = false;
                        finishOtaProgress(false, "Could not open temporary web package file.");
                        setWebBundleUploadResult(500, "error", "Could not open temporary web package file.", false);
                        releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                        return;
                      }
                    }

                    localOtaLastActivityMillis = millis();

                    if (len && (!request->_tempFile || request->_tempFile.write(data, len) != len)) {
                      if (request->_tempFile) {
                        request->_tempFile.close();
                      }
                      LittleFS.remove(WEB_BUNDLE_UPLOAD_PATH);
                      localOtaInProgress = false;
                      finishOtaProgress(false, "Web package upload write failed.");
                      setWebBundleUploadResult(500, "error", "Web package upload write failed.", false);
                      releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                      return;
                    }

                    updateOtaProgress("upload-web-package",
                                      index + len,
                                      request->contentLength(),
                                      "Uploading web package...");

                    if (final) {
                      if (request->_tempFile) {
                        request->_tempFile.close();
                      }

                      updateOtaProgress("install-web-package", request->contentLength(), request->contentLength(), "Installing web package...");
                      String installedVersion;
                      String errorMessage;
                      const bool success = installWebBundleFromTar(WEB_BUNDLE_UPLOAD_PATH, installedVersion, errorMessage);
                      LittleFS.remove(WEB_BUNDLE_UPLOAD_PATH);

                      if (!success) {
                        localOtaInProgress = false;
                        localOtaLastActivityMillis = 0;
                        finishOtaProgress(false, errorMessage.length() ? errorMessage : "Web package installation failed.");
                        setWebBundleUploadResult(500, "error", errorMessage.length() ? errorMessage : "Web package installation failed.", false);
                        releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                        return;
                      }

                      localOtaInProgress = false;
                      localOtaLastActivityMillis = 0;
                      const String successMessage = "Web package installed successfully. Stored web file version: " + getStoredWebFilesVersion() + ".";
                      finishOtaProgress(true, successMessage);
                      setWebBundleUploadResult(200, "ok", successMessage, true);
                      releaseMaintenanceOperation(MaintenanceOperation::WebBundle);
                    }
                  }
  );

  httpServer.on("/formatfs", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    if (!acquireMaintenanceOperation(MaintenanceOperation::FormatFilesystem)) {
      request->send(409, "application/json", "{\"status\":\"error\",\"message\":\"Device maintenance is busy.\"}");
      return;
    }
    formatfs(LittleFS);
    releaseMaintenanceOperation(MaintenanceOperation::FormatFilesystem);
    request->send(200, "text/html", "done");
  });

  httpServer.on("/updatefiles", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      setWebFilesUpdateError(true, "No WiFi connection available for web files download.");
      request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"No WiFi connection available for web files download.\"}");
      return;
    }
    if (getConfiguredFirmwareBaseUrl().length() == 0) {
      setWebFilesUpdateError(true, "MDS OTA endpoint is not configured.");
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"MDS OTA endpoint is not configured.\"}");
      return;
    }
    String queueError;
    if (!queueWebFilesDownloadTask(queueError)) {
      request->send(409, "application/json", "{\"status\":\"error\",\"message\":\"" + jsonEscape(queueError) + "\"}");
      return;
    }
    request->send(200, "application/json", "{\"status\":\"queued\",\"message\":\"Web files download queued.\"}");
  });

  httpServer.on("/updatefilesstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    String test = String(runDownloadingFilesStatus || runDownloadingFiles);
    request->send(200, "text/html", test);
  });

  httpServer.on("/updatefilesprogress", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    request->send(200, "application/json", buildUpdateFilesProgressJson());
  });

  httpServer.on("/updatefilesinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    request->send(200, "application/json", buildUpdateFilesProgressJson());
  });

  httpServer.on("/otaprogress", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    request->send(200, "application/json", buildOtaProgressJson());
  });

  httpServer.on("/mdsotainfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    const String requestedChannel = normalizeManifestChannel(request->hasParam("channel")
      ? request->getParam("channel")->value()
      : getFirmwareReleaseChannel());
    if (requestedChannel.length() == 0) {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown firmware channel requested.\"}");
      return;
    }
    OtaMetadataSnapshot state = getOtaMetadataSnapshot(requestedChannel);
    const bool refresh = request->hasParam("refresh") && request->getParam("refresh")->value() == "1";
    if ((refresh || !state.complete) && !state.running) {
      String errorMessage;
      queueOtaMetadata(requestedChannel, errorMessage);
      state = getOtaMetadataSnapshot(requestedChannel);
      if (!state.running && !state.complete) {
        JsonDocument error;
        error["running"] = false;
        error["status"] = "error";
        error["message"] = errorMessage;
        String body;
        serializeJson(error, body);
        request->send(409, "application/json", body);
        return;
      }
    }
    if (state.running) {
      request->send(202, "application/json", "{\"running\":true,\"message\":\"OTA metadata check is running.\"}");
      return;
    }
    request->send(200, "application/json", state.json.length() ? state.json : "{\"status\":\"error\",\"message\":\"OTA metadata is unavailable.\"}");
  });

  httpServer.on("/otadiagnostics", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    OtaDiagnosticsSnapshot state = getOtaDiagnosticsSnapshot();
    const bool refresh = request->hasParam("refresh") && request->getParam("refresh")->value() == "1";
    if ((refresh || !state.complete) && !state.running) {
      String errorMessage;
      queueOtaDiagnostics(errorMessage);
      state = getOtaDiagnosticsSnapshot();
      if (!state.running && !state.complete) {
        JsonDocument error;
        error["running"] = false;
        error["complete"] = false;
        error["status"] = "error";
        error["message"] = errorMessage;
        String body;
        serializeJson(error, body);
        request->send(409, "application/json", body);
        return;
      }
    }
    if (state.running) {
      request->send(202, "application/json", "{\"running\":true,\"complete\":false,\"message\":\"OTA diagnostics are running.\"}");
      return;
    }
    request->send(200, "application/json", state.json.length() ? state.json : "{\"running\":false,\"complete\":false}");
  });

  httpServer.on("/testMdsUpload", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"No Wi-Fi connection available for MDS test.\"}");
      return;
    }

    String queueError;
    if (!queueMdsTest(queueError)) {
      request->send(409, "application/json", "{\"status\":\"error\",\"message\":\"" + jsonEscape(queueError) + "\"}");
      return;
    }

    JsonDocument response;
    response["status"] = "queued";
    response["message"] = "MDS test upload queued.";
    String body;
    serializeJson(response, body);
    request->send(202, "application/json", body);
  });

  httpServer.on("/testMdsUploadStatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuthenticatedRequest(request)) return;
    const MdsTestSnapshot state = getMdsTestSnapshot();
    JsonDocument response;
    response["running"] = state.running;
    response["complete"] = state.complete;
    response["success"] = state.success;
    response["message"] = state.message;
    response["detail"] = state.detail;
    String body;
    serializeJson(response, body);
    request->send(200, "application/json", body);
  });

}

#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

size_t content_len;

void maintainLocalOtaUpload() {
  if (!localOtaInProgress || localOtaLastActivityMillis == 0 ||
      millis() - localOtaLastActivityMillis <= LOCAL_OTA_STALL_TIMEOUT_MS) {
    return;
  }

  DebugPrintln(1, "Local update upload timed out; clearing update state");
  if (localOtaUpdateWriterActive) {
    Update.abort();
  }
  localOtaUpdateWriterActive = false;
  localOtaLastActivityMillis = 0;
  localOtaInProgress = false;
  LittleFS.remove(WEB_BUNDLE_UPLOAD_PATH);
  cleanupStaleWebBundleArtifactsInternal();
  finishOtaProgress(false, "Local update upload timed out. Please try again.");
  const MaintenanceOperation operation = getMaintenanceOperation();
  if (operation == MaintenanceOperation::LocalFirmware || operation == MaintenanceOperation::WebBundle) {
    releaseMaintenanceOperation(operation);
  }
}

namespace {
int otaServerLogLastBucket = -1;
String otaServerLogLastPhase = "";
unsigned long otaServerLogLastMillis = 0;
bool otaServerStatusPending = false;
String otaServerPendingPhase;
String otaServerPendingVersion;
String otaServerPendingMessage;
int otaServerPendingPercent = 0;

int otaProgressPercent(size_t currentBytes, size_t totalBytes) {
  if (totalBytes == 0) {
    return 0;
  }
  return static_cast<int>((currentBytes * 100) / totalBytes);
}

String otaServerTargetVersion() {
  const RemoteOtaSnapshot state = getRemoteOtaSnapshot();
  if (state.request.version.length() > 0) {
    return state.request.version;
  }
  return String(actconf.fversion);
}

void logRemoteOtaStatusToMds(const String &phase,
                             size_t currentBytes,
                             size_t totalBytes,
                             const String &message,
                             bool force = false) {
  if (!getRemoteOtaSnapshot().inProgress) {
    return;
  }

  const int percent = otaProgressPercent(currentBytes, totalBytes);
  const int bucket = percent >= 100 ? 100 : (percent / 25) * 25;
  const unsigned long now = millis();
  const bool phaseChanged = phase != otaServerLogLastPhase;
  const bool bucketChanged = bucket != otaServerLogLastBucket;
  const bool timeElapsed = now - otaServerLogLastMillis >= 15000UL;

  if (!force && !phaseChanged && !bucketChanged && !timeElapsed) {
    return;
  }

  otaServerLogLastPhase = phase;
  otaServerLogLastBucket = bucket;
  otaServerLogLastMillis = now;
  otaServerStatusPending = true;
  otaServerPendingPhase = phase;
  otaServerPendingPercent = percent;
  otaServerPendingVersion = otaServerTargetVersion();
  otaServerPendingMessage = message;
}

void resetOtaProgress() {
  setOtaProgressState(OtaProgressSnapshot());
  otaServerLogLastBucket = -1;
  otaServerLogLastPhase = "";
  otaServerLogLastMillis = 0;
  otaServerStatusPending = false;
}

void flushPendingRemoteOtaStatusInternal() {
  if (!otaServerStatusPending || WiFi.status() != WL_CONNECTED) return;
  const String phase = otaServerPendingPhase;
  const String version = otaServerPendingVersion;
  const String message = otaServerPendingMessage;
  const int percent = otaServerPendingPercent;
  otaServerStatusPending = false;
  sendMdsOtaStatus(actconf, phase.c_str(), percent, version, message);
}

void startOtaProgress(const String &phase, size_t totalBytes, const String &message) {
  OtaProgressSnapshot state;
  state.active = true;
  state.total = totalBytes;
  state.phase = phase;
  state.message = message;
  setOtaProgressState(state);
  const String title = phase.indexOf("filesystem") >= 0 ? "FS Update" : "OTA Update";
  writeDisplayProgressScreen(title, message, 0, totalBytes, phase);
  logRemoteOtaStatusToMds(phase, 0, totalBytes, message, true);
}

void updateOtaProgress(const String &phase, size_t currentBytes, size_t totalBytes, const String &message) {
  OtaProgressSnapshot state = getOtaProgressSnapshot();
  state.active = true;
  state.current = currentBytes;
  if (totalBytes > 0) state.total = totalBytes;
  state.phase = phase;
  state.message = message;
  setOtaProgressState(state);
  const String title = phase.indexOf("filesystem") >= 0 ? "FS Update" : "OTA Update";
  writeDisplayProgressScreen(title, message, state.current, state.total, phase);
  logRemoteOtaStatusToMds(phase, state.current, state.total, message);
}

void finishOtaProgress(bool success, const String &message) {
  OtaProgressSnapshot state = getOtaProgressSnapshot();
  state.active = false;
  state.success = success;
  if (state.total > 0) state.current = state.total;
  state.phase = success ? "complete" : "error";
  state.message = message;
  setOtaProgressState(state);
  writeDisplayStatusScreen(success ? "Update complete" : "Update error",
                           message,
                           state.total > 0 ? (String(state.total) + " bytes") : "",
                           success ? "Reboot pending" : "See WebSerial");
  logRemoteOtaStatusToMds(state.phase, state.current, state.total, message, true);
}

String buildOtaProgressJson() {
  const OtaProgressSnapshot state = getOtaProgressSnapshot();
  JsonDocument json;
  json["active"] = state.active;
  json["success"] = state.success;
  json["phase"] = state.phase;
  json["message"] = state.message;
  json["current"] = state.current;
  json["total"] = state.total;
  json["percent"] = state.total > 0 ? (state.current * 100) / state.total : 0;

  String response;
  serializeJson(json, response);
  return response;
}

String buildUpdateFilesProgressJson() {
  JsonDocument json;
  const WebFilesUpdateSnapshot state = getWebFilesUpdateSnapshot();
  const bool busy = runDownloadingFilesStatus || runDownloadingFiles;
  const bool sourceConfigured = getConfiguredFirmwareBaseUrl().length() > 0;
  const bool retrying = runDownloadingFiles && !runDownloadingFilesStatus && state.retryCount > 0;

  // This endpoint is polled frequently by multiple pages. Never perform TLS or
  // manifest I/O here; the update worker reports a concrete error if the
  // configured server does not contain a matching package.
  const bool serverSupportsInstalledFirmware = sourceConfigured;

  json["busy"] = busy;
  json["configured"] = sourceConfigured;
  json["retrying"] = retrying;
  json["serverSupportsInstalledFirmware"] = serverSupportsInstalledFirmware;
  json["firmwareVersion"] = String(actconf.fversion);
    json["firmwareChannel"] = getFirmwareReleaseChannel();
    json["firmwareChannelLabel"] = getFirmwareReleaseLabel();
    json["standbyAutoUpdate"] = String(actconf.standbyAutoUpdate);
    json["standbyAutoUpdateIntervalHours"] = actconf.standbyAutoUpdateIntervalHours;
  json["storedWebFilesVersion"] = getStoredWebFilesVersionOnly();
  json["storedWebFilesChannel"] = getStoredWebFilesChannel();
  json["upToDate"] = sourceConfigured ? areWebFilesCurrent(actconf.fversion, getFirmwareReleaseChannel().c_str()) : false;
  json["completed"] = state.completed;
  json["total"] = state.total;
  json["currentFile"] = state.currentName;
  json["progressUnit"] = state.currentName == "webui-package.tar" && state.total > 1 ? "bytes" : "files";
  json["message"] = sourceConfigured
    ? state.message
    : "MDS OTA endpoint is not configured.";
  json["error"] = state.error;
  json["retryCount"] = state.retryCount;
  json["percent"] = state.total > 0 ? (state.completed * 100) / state.total : 0;

  String response;
  serializeJson(json, response);
  return response;
}
}

void flushPendingRemoteOtaStatus() {
  flushPendingRemoteOtaStatusInternal();
}

static bool isFilesystemUpdateRequest(AsyncWebServerRequest *request, const String& filename) {
  if (request != nullptr) {
    if (request->hasParam("filesystem", true, true) || request->hasParam("filesystem", true)) {
      return true;
    }
  }
  return false;
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage) {
  return performRemoteOtaUpdate(url, filesystemUpdate, errorMessage, "", false);
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256) {
  return performRemoteOtaUpdate(url, filesystemUpdate, errorMessage, expectedSha256, false);
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256, bool useMdsOtaEndpoint) {
  if (!url.startsWith("https://")) {
    errorMessage = "Remote update URLs must use HTTPS.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  String normalizedExpectedSha256 = normalizeSha256String(expectedSha256);
  if (!filesystemUpdate && expectedSha256.length() > 0 && normalizedExpectedSha256.length() == 0) {
    errorMessage = "Remote firmware checksum is invalid.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  if (!filesystemUpdate && !saveConfigBackupToLittleFS(actconf)) {
    errorMessage = "Config backup failed. Firmware update cancelled.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  WiFiClientSecure client;
  HTTPClient http;
  const RemoteOtaRequest otaRequest = getRemoteOtaSnapshot().request;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(useMdsOtaEndpoint ? HTTPC_DISABLE_FOLLOW_REDIRECTS : HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (useMdsOtaEndpoint) {
    const char *responseHeaders[] = {
      "x-SHA256",
      "X-SHA256",
      "x-MD5",
      "X-MD5",
      "x-Firmware-Version",
      "X-Firmware-Version"
    };
    http.collectHeaders(responseHeaders, 6);
    http.setUserAgent("ESP32-http-Update");
  }

  if (!http.begin(client, url)) {
    errorMessage = "Unable to initialize remote HTTPS update request.";
    finishOtaProgress(false, errorMessage);
    return false;
  }
  if (useMdsOtaEndpoint) {
    http.addHeader("X-MDS-OTA-Secret", String(actconf.mdsOtaSecret));
    http.addHeader("x-ESP32-STA-MAC", WiFi.macAddress());
    http.addHeader("x-ESP32-sketch-md5", ESP.getSketchMD5());
    http.addHeader("x-ESP32-sdk-version", String(ESP.getSdkVersion()));
    http.addHeader("x-ESP32-version", String(actconf.fversion));
    if (otaRequest.channel.length() > 0) {
      http.addHeader("x-ESP32-OTA-Channel", otaRequest.channel);
    }
    if (otaRequest.forceReinstall) {
      http.addHeader("x-ESP32-OTA-Force", "1");
    }
  }

  startOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware", 0,
                   filesystemUpdate ? "Downloading file system update from server..." : "Downloading firmware from server...");
  const int httpCode = http.GET();
  if (useMdsOtaEndpoint && httpCode == HTTP_CODE_NOT_MODIFIED) {
    OtaProgressSnapshot state;
    state.success = true;
    state.phase = "no-update";
    state.message = "MDS reports no newer firmware available.";
    setOtaProgressState(state);
    writeDisplayStatusScreen("OTA Status", "Firmware current", "MDS current");
    http.end();
    return true;
  }
  if (httpCode != HTTP_CODE_OK) {
    errorMessage = "Remote update download failed: HTTP " + String(httpCode);
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  const int cmd = filesystemUpdate ? U_PART : U_FLASH;
  const int contentLength = http.getSize();
  String expectedMd5 = "";
  if (useMdsOtaEndpoint) {
    String serverFirmwareVersion = http.header("x-Firmware-Version");
    if (serverFirmwareVersion.length() == 0) {
      serverFirmwareVersion = http.header("X-Firmware-Version");
    }
    serverFirmwareVersion.trim();
    if (!filesystemUpdate && serverFirmwareVersion.length() == 0) {
      errorMessage = "MDS OTA response is missing the firmware version header.";
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }
    if (!filesystemUpdate) {
      const int versionComparison = compareFirmwareVersions(serverFirmwareVersion, String(actconf.fversion));
      if (versionComparison == 0 && !getRemoteOtaSnapshot().request.forceReinstall) {
        OtaProgressSnapshot state;
        state.success = true;
        state.phase = "no-update";
        state.message = "MDS returned the installed firmware version. Update skipped.";
        setOtaProgressState(state);
        writeDisplayStatusScreen("OTA Status", "Firmware current", serverFirmwareVersion);
        DebugPrintln(2, "Skipping MDS OTA because server returned installed firmware version: " + serverFirmwareVersion);
        http.end();
        return true;
      }
      if (versionComparison < 0) {
        errorMessage = "MDS returned older firmware " + serverFirmwareVersion + ". Update cancelled.";
        finishOtaProgress(false, errorMessage);
        http.end();
        return false;
      }
      setRemoteOtaVersion(serverFirmwareVersion);
    }

    normalizedExpectedSha256 = normalizeSha256String(http.header("x-SHA256"));
    if (normalizedExpectedSha256.length() == 0) {
      normalizedExpectedSha256 = normalizeSha256String(http.header("X-SHA256"));
    }
    if (normalizedExpectedSha256.length() == 0) {
      errorMessage = "MDS OTA response is missing a valid SHA256 checksum.";
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }

    expectedMd5 = http.header("x-MD5");
    if (expectedMd5.length() == 0) {
      expectedMd5 = http.header("X-MD5");
    }
    expectedMd5.trim();
  }
  if (contentLength > 0) {
    startOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware",
                     static_cast<size_t>(contentLength),
                     filesystemUpdate ? "Downloading file system update from server..." : "Downloading firmware from server...");
  }
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
    Update.printError(Serial);
    errorMessage = "Unable to start OTA update.";
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }
  if (useMdsOtaEndpoint && expectedMd5.length() == 32) {
    Update.setMD5(expectedMd5.c_str());
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  unsigned long lastReadMs = millis();
  mbedtls_sha256_context shaContext;
  const bool verifySha256 = !filesystemUpdate && normalizedExpectedSha256.length() == 64;
  if (verifySha256) {
    mbedtls_sha256_init(&shaContext);
    mbedtls_sha256_starts_ret(&shaContext, 0);
  }
  while (http.connected() && (contentLength <= 0 || static_cast<int>(written) < contentLength)) {
    const size_t availableBytes = stream->available();
    if (availableBytes == 0) {
      if (!http.connected() && !stream->available()) {
        break;
      }
      if (millis() - lastReadMs > 15000) {
        break;
      }
      delay(1);
      continue;
    }

    const size_t toRead = availableBytes > sizeof(buffer) ? sizeof(buffer) : availableBytes;
    const size_t bytesRead = stream->readBytes(buffer, toRead);
    if (bytesRead == 0) {
      continue;
    }

    lastReadMs = millis();
    if (Update.write(buffer, bytesRead) != bytesRead) {
      Update.printError(Serial);
      errorMessage = "Downloaded firmware could not be written completely.";
      Update.abort();
      if (verifySha256) {
        mbedtls_sha256_free(&shaContext);
      }
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }

    if (verifySha256) {
      mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
    }
    written += bytesRead;
    updateOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware",
                      written,
                      contentLength > 0 ? static_cast<size_t>(contentLength) : 0,
                      filesystemUpdate ? "Downloading and writing file system update..." : "Downloading and writing firmware...");
  }

  if (written == 0 || (contentLength > 0 && static_cast<int>(written) != contentLength)) {
    Update.printError(Serial);
    errorMessage = "Downloaded firmware could not be written completely.";
    Update.abort();
    if (verifySha256) {
      mbedtls_sha256_free(&shaContext);
    }
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  if (verifySha256) {
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&shaContext, digest);
    mbedtls_sha256_free(&shaContext);

    const String actualSha256 = sha256ToHexString(digest);
    if (actualSha256 != normalizedExpectedSha256) {
      errorMessage = "Remote firmware checksum mismatch.";
      Update.abort();
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }
  }

  updateOtaProgress("finalizing", written, written, "Finalizing update...");
  if (!Update.end(true)) {
    Update.printError(Serial);
    errorMessage = "Update finalize failed.";
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  if (filesystemUpdate) {
    saveWebFilesVersion(actconf.fversion);
  }

  http.end();
  keepAwakeAfterUpdateRestart = true;
  setRemoteOtaRebootRequired(true);
  finishOtaProgress(true, filesystemUpdate ? "File system update complete. Device reboots now." : "Firmware update complete. Device reboots now.");
  return true;
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  const bool filesystemUpdate = isFilesystemUpdateRequest(request, filename);

  if (!index){
    if (!acquireMaintenanceOperation(MaintenanceOperation::LocalFirmware)) {
      finishOtaProgress(false, "Device maintenance is busy.");
      request->send(409, "application/json", buildOtaResponse("error", "Device maintenance is busy.", false, false, false));
      return;
    }
    Serial.println("Update");
    content_len = request->contentLength();
    localOtaInProgress = true;
    localOtaUpdateWriterActive = false;
    localOtaLastActivityMillis = millis();
    standbySleepBlockedUntilMillis = millis() + 180000UL;
    startOtaProgress(filesystemUpdate ? "upload-filesystem" : "upload-firmware", content_len,
                     filesystemUpdate ? "Uploading file system update..." : "Uploading firmware...");
    // Detect filesystem uploads both from modern field names and legacy filenames.
    int cmd = filesystemUpdate ? U_PART : U_FLASH;
    if (!filesystemUpdate && !saveConfigBackupToLittleFS(actconf)) {
      localOtaInProgress = false;
      finishOtaProgress(false, "Config backup failed. Firmware update cancelled.");
      releaseMaintenanceOperation(MaintenanceOperation::LocalFirmware);
      request->send(500, "application/json", buildOtaResponse("error", "Config backup failed. Firmware update cancelled.", false, false, false));
      return;
    }
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
      localOtaInProgress = false;
      finishOtaProgress(false, "Unable to start OTA update.");
      releaseMaintenanceOperation(MaintenanceOperation::LocalFirmware);
      request->send(500, "application/json", buildOtaResponse("error", "Unable to start OTA update.", false, false, !filesystemUpdate));
      return;
    }
    localOtaUpdateWriterActive = true;
  }

  localOtaLastActivityMillis = millis();
  const size_t written = Update.write(data, len);
  if (written != len) {
    Update.printError(Serial);
    Update.abort();
    localOtaInProgress = false;
    localOtaUpdateWriterActive = false;
    localOtaLastActivityMillis = 0;
    finishOtaProgress(false, "Update write failed.");
    releaseMaintenanceOperation(MaintenanceOperation::LocalFirmware);
    request->send(500, "application/json", buildOtaResponse("error", "Update write failed.", false, false, !filesystemUpdate));
    return;
  }
#ifdef ESP8266
  if (Update.size() > 0) {
    Serial.printf("Progress: %d%%\n", (Update.progress() * 100) / Update.size());
  }
#endif
  updateOtaProgress(filesystemUpdate ? "upload-filesystem" : "upload-firmware",
                    index + len,
                    content_len,
                    filesystemUpdate ? "Uploading file system update..." : "Uploading firmware...");
  //Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
  if (final) {
    updateOtaProgress("finalizing", content_len, content_len, "Finalizing update...");
    if (!Update.end(true)){
      Update.printError(Serial);
      localOtaInProgress = false;
      localOtaUpdateWriterActive = false;
      localOtaLastActivityMillis = 0;
      finishOtaProgress(false, "Update failed. See serial log for details.");
      releaseMaintenanceOperation(MaintenanceOperation::LocalFirmware);
      request->send(500, "application/json", buildOtaResponse("error", "Update failed. See serial log for details.", false, false, !filesystemUpdate));
    } else {
      if (filesystemUpdate) {
        saveWebFilesVersion(actconf.fversion);
      }
      keepAwakeAfterUpdateRestart = true;
      Serial.println("Update complete");
      const String message = filesystemUpdate
        ? "FileSystem update complete. Device reboots now."
        : "Firmware update complete. Device reboots now.";
      finishOtaProgress(true, message);
      request->send(200, "application/json", buildOtaResponse("ok", message, true, false, !filesystemUpdate));
      localOtaInProgress = false;
      localOtaUpdateWriterActive = false;
      localOtaLastActivityMillis = 0;
      releaseMaintenanceOperation(MaintenanceOperation::LocalFirmware);
      scheduledRestartMillis = millis() + 2000UL;
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}
