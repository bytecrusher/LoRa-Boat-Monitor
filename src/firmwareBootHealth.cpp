#include "firmwareBootHealth.h"

#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace {
constexpr char NAMESPACE[] = "fwhealth";
constexpr char KEY_PENDING[] = "pending";
constexpr char KEY_TARGET[] = "target";
constexpr char KEY_PREVIOUS[] = "previous";
constexpr char KEY_ATTEMPTS[] = "attempts";
constexpr char KEY_RESULT[] = "result";
constexpr uint8_t MAX_BOOT_ATTEMPTS = 3;

RTC_DATA_ATTR uint8_t consecutiveCrashBoots = 0;
bool validationPending = false;
bool recoverySafeMode = false;
uint8_t bootAttempts = 0;
String bootHealthStatus = "not initialized";

bool isCrashReset(esp_reset_reason_t reason) {
  return reason == ESP_RST_PANIC ||
         reason == ESP_RST_INT_WDT ||
         reason == ESP_RST_TASK_WDT ||
         reason == ESP_RST_WDT ||
         reason == ESP_RST_BROWNOUT;
}

const esp_partition_t *findAppPartitionByLabel(const String &label) {
  if (label.length() == 0) return nullptr;
  return esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    label.c_str()
  );
}

void clearPendingValidation(Preferences &preferences, const String &result) {
  preferences.putBool(KEY_PENDING, false);
  preferences.putUChar(KEY_ATTEMPTS, 0);
  preferences.putString(KEY_RESULT, result);
  validationPending = false;
  bootAttempts = 0;
}
}

void initializeFirmwareBootHealth(const char *currentVersion) {
  const esp_reset_reason_t resetReason = esp_reset_reason();
  if (isCrashReset(resetReason)) {
    if (consecutiveCrashBoots < UINT8_MAX) ++consecutiveCrashBoots;
  } else if (resetReason != ESP_RST_DEEPSLEEP) {
    consecutiveCrashBoots = 0;
  }
  recoverySafeMode = consecutiveCrashBoots >= MAX_BOOT_ATTEMPTS;

  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) {
    bootHealthStatus = "NVS unavailable";
    return;
  }

  validationPending = preferences.getBool(KEY_PENDING, false);
  const String targetVersion = preferences.getString(KEY_TARGET, "");
  const String runningVersion = currentVersion == nullptr ? "" : String(currentVersion);
  if (!validationPending) {
    bootHealthStatus = preferences.getString(KEY_RESULT, "healthy");
    preferences.end();
    return;
  }

  if (targetVersion.length() > 0 && targetVersion != runningVersion) {
    clearPendingValidation(preferences, "rollback active: " + runningVersion);
    bootHealthStatus = "running previous firmware after rollback";
    preferences.end();
    return;
  }

  if (resetReason != ESP_RST_DEEPSLEEP) {
    bootAttempts = preferences.getUChar(KEY_ATTEMPTS, 0) + 1;
    preferences.putUChar(KEY_ATTEMPTS, bootAttempts);
  } else {
    bootAttempts = preferences.getUChar(KEY_ATTEMPTS, 0);
  }

  bootHealthStatus = "awaiting boot confirmation (attempt " + String(bootAttempts) + ")";
  if (bootAttempts < MAX_BOOT_ATTEMPTS || !isCrashReset(resetReason)) {
    preferences.end();
    return;
  }

  const String previousLabel = preferences.getString(KEY_PREVIOUS, "");
  const esp_partition_t *previousPartition = findAppPartitionByLabel(previousLabel);
  if (previousPartition == nullptr || esp_ota_set_boot_partition(previousPartition) != ESP_OK) {
    recoverySafeMode = true;
    bootHealthStatus = "rollback failed; recovery mode active";
    preferences.putString(KEY_RESULT, bootHealthStatus);
    preferences.end();
    return;
  }

  clearPendingValidation(preferences, "automatic rollback requested");
  bootHealthStatus = "automatic rollback requested";
  preferences.end();
  delay(100);
  ESP.restart();
}

void armFirmwareBootValidation(const char *targetVersion) {
  const esp_partition_t *runningPartition = esp_ota_get_running_partition();
  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return;
  preferences.putBool(KEY_PENDING, true);
  preferences.putString(KEY_TARGET, targetVersion == nullptr ? "" : targetVersion);
  preferences.putString(KEY_PREVIOUS, runningPartition == nullptr ? "" : runningPartition->label);
  preferences.putUChar(KEY_ATTEMPTS, 0);
  preferences.putString(KEY_RESULT, "update installed; validation pending");
  preferences.end();
}

void confirmFirmwareBootHealthy() {
  if (!validationPending) return;

  Preferences preferences;
  if (!preferences.begin(NAMESPACE, false)) return;
  clearPendingValidation(preferences, "healthy");
  preferences.end();
  esp_ota_mark_app_valid_cancel_rollback();
  consecutiveCrashBoots = 0;
  bootHealthStatus = "healthy";
}

bool isFirmwareBootValidationPending() {
  return validationPending;
}

bool isRecoverySafeMode() {
  return recoverySafeMode;
}

uint8_t getFirmwareBootAttemptCount() {
  return bootAttempts;
}

String getFirmwareBootHealthStatus() {
  return bootHealthStatus;
}
