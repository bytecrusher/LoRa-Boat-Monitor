#include "func_myFunctions.h"
#include <Configuration.h>
#include <WiFi.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
struct ConfigStorageHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t length;
  uint32_t checksum;
};

struct ConfigBackupHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t length;
  uint32_t checksum;
};

constexpr uint32_t CONFIG_STORAGE_MAGIC = 0x43464731UL;  // "CFG1"
constexpr uint16_t CONFIG_STORAGE_VERSION = 1;
constexpr uint32_t CONFIG_BACKUP_MAGIC = 0x43424631UL;   // "CBF1"
constexpr uint16_t CONFIG_BACKUP_VERSION = 1;
constexpr char CONFIG_BACKUP_PATH[] = "/config-backup.bin";
constexpr char WEB_FILES_VERSION_PATH[] = "/webfiles-version.txt";
StaticSemaphore_t configStorageMutexBuffer;
SemaphoreHandle_t configStorageMutex = nullptr;

class ConfigStorageGuard {
public:
  ConfigStorageGuard() {
    if (configStorageMutex == nullptr) {
      configStorageMutex = xSemaphoreCreateMutexStatic(&configStorageMutexBuffer);
    }
    locked = configStorageMutex != nullptr && xSemaphoreTake(configStorageMutex, portMAX_DELAY) == pdTRUE;
  }

  ~ConfigStorageGuard() {
    if (locked) xSemaphoreGive(configStorageMutex);
  }

  bool acquired() const { return locked; }

private:
  bool locked = false;
};

int configHeaderStart() {
  return cfgStart - int(sizeof(ConfigStorageHeader));
}

uint32_t calculateConfigChecksum(const uint8_t *data, size_t len) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool readConfigStorageHeader(ConfigStorageHeader &header) {
  if (configHeaderStart() < 0) {
    return false;
  }
  EEPROM.get(configHeaderStart(), header);

  return header.magic == CONFIG_STORAGE_MAGIC &&
         header.version == CONFIG_STORAGE_VERSION &&
         header.length > 0 &&
         header.length <= sizeof(configData);
}

bool migrateConfigWithExpandedFirmwareVersion(const uint8_t *storedConfig, uint16_t storedLength, configData &cfg) {
  constexpr size_t oldFirmwareVersionSize = 7;
  const size_t firmwareOffset = offsetof(configData, fversion);
  const size_t newFirmwareVersionSize = sizeof(cfg.fversion);
  const size_t oldTailOffset = firmwareOffset + oldFirmwareVersionSize;
  const size_t newTailOffset = firmwareOffset + newFirmwareVersionSize;

  if (newFirmwareVersionSize != oldFirmwareVersionSize + 1 ||
      storedLength != sizeof(configData) - 1 ||
      oldTailOffset > storedLength ||
      newTailOffset > sizeof(configData)) {
    return false;
  }

  int storedValid = 0;
  memcpy(&storedValid, storedConfig, sizeof(storedValid));
  if (storedValid != 17) {
    return false;
  }

  cfg = configData();
  memcpy(reinterpret_cast<uint8_t*>(&cfg), storedConfig, firmwareOffset);
  memcpy(cfg.fversion, storedConfig + firmwareOffset, min(oldFirmwareVersionSize, newFirmwareVersionSize - 1));
  cfg.fversion[newFirmwareVersionSize - 1] = '\0';

  const size_t tailLength = storedLength - oldTailOffset;
  if (tailLength > sizeof(configData) - newTailOffset) {
    return false;
  }
  memcpy(reinterpret_cast<uint8_t*>(&cfg) + newTailOffset, storedConfig + oldTailOffset, tailLength);
  cfg.valid = defconf.valid;
  return true;
}

bool readConfigBackupHeader(File &file, ConfigBackupHeader &header) {
  if (!file || file.size() < int(sizeof(ConfigBackupHeader) + sizeof(configData))) {
    return false;
  }

  if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    return false;
  }

  return header.magic == CONFIG_BACKUP_MAGIC &&
         header.version == CONFIG_BACKUP_VERSION &&
         header.length == sizeof(configData);
}

String htmlEscapeLocal(const String &value) {
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

String buildMacScopedDefaultSecret(const char *prefix, uint64_t suffixValue, uint8_t hexDigits) {
  if (prefix == nullptr || hexDigits == 0 || hexDigits > 12) {
    return "";
  }

  char format[10];
  snprintf(format, sizeof(format), "%%0%dllX", hexDigits);

  char suffix[20];
  snprintf(suffix, sizeof(suffix), format, static_cast<unsigned long long>(suffixValue));

  String secret = String(prefix) + String(suffix);
  secret.toUpperCase();
  return secret;
}

bool hasExpectedGeneratedPrefix(const char *value, const char *prefix, size_t expectedLength) {
  if (value == nullptr || prefix == nullptr) {
    return false;
  }

  const size_t prefixLength = strlen(prefix);
  if (strncmp(value, prefix, prefixLength) != 0 || strlen(value) != expectedLength) {
    return false;
  }

  for (size_t i = prefixLength; value[i] != '\0'; i++) {
    if (!isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }

  return true;
}
}  // namespace

// Debugging functions
void DebugPrintln(int type, const char* x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, char x[]){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, float x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, char x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, int x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, uint32_t x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, unsigned long x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, String x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrintln(int type, IPAddress x){
  if(type <= actconf.debug){
    Serial.println(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.println(x);
    }
  }
}

void DebugPrint(int type, const char* x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, char x[]){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, float x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, char x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, int x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, uint32_t x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, unsigned long x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, String x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, IPAddress x){
  if(type <= actconf.debug){
    Serial.print(x);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(x);
    }
  }
}

void DebugPrint(int type, unsigned int num, int base){
  if(type <= actconf.debug){
    Serial.println(num, base);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.print(num, base);
    }
  }
}

void eraseEEPROMConfig(const configData &cfg) {
  ConfigStorageGuard guard;
  if (!guard.acquired()) {
    DebugPrintln(1, "Could not lock configuration storage for erase");
    return;
  }
  // Reset EEPROM bytes to '0' for the length of the data structure
  //noInterrupts();                       // Stop all interrupts important for writing in EEPROM
  EEPROM.begin(sizeEEPROM);
  const int eraseStart = max(0, configHeaderStart());
  const int eraseEnd = cfgStart + sizeof(cfg);
  for (int i = eraseStart; i < eraseEnd; i++) {
    EEPROM.write(i, 0);
  }
  const bool committed = EEPROM.commit();
  EEPROM.end();
  if (!committed) DebugPrintln(1, "Failed to erase EEPROM configuration");
  //interrupts();                         // Activate all interrupts
}

bool saveEEPROMConfig(const configData &cfg) {
  ConfigStorageGuard guard;
  if (!guard.acquired()) {
    DebugPrintln(1, "Could not lock configuration storage for save");
    return false;
  }
  // Save configuration from RAM into EEPROM
  //noInterrupts();                       // Stop all interrupts important for writing in EEPROM
  const ConfigStorageHeader header = {
    CONFIG_STORAGE_MAGIC,
    CONFIG_STORAGE_VERSION,
    uint16_t(sizeof(configData)),
    calculateConfigChecksum(reinterpret_cast<const uint8_t*>(&cfg), sizeof(configData))
  };

  EEPROM.begin(sizeEEPROM);
  EEPROM.put(configHeaderStart(), header);
  EEPROM.put( cfgStart, cfg );
  const bool committed = EEPROM.commit();
  EEPROM.end();                         // Free RAM copy of structure
  //interrupts();                         // Activate all interrupts
  DebugPrintln(committed ? 3 : 1, committed ? "New settings saved in EEPROM" : "Failed to commit EEPROM configuration");
  return committed;
}

configData loadEEPROMConfig() {
  ConfigStorageGuard guard;
  if (!guard.acquired()) {
    DebugPrintln(1, "Could not lock configuration storage for load");
    return configData();
  }
  // Loads configuration from EEPROM into RAM
  configData cfg = configData();
  ConfigStorageHeader header;
  EEPROM.begin(sizeEEPROM);
  if (readConfigStorageHeader(header)) {
    uint8_t storedConfig[sizeof(configData)] = {0};
    for (uint16_t i = 0; i < header.length; ++i) {
      storedConfig[i] = EEPROM.read(cfgStart + i);
    }
    EEPROM.end();

    const uint32_t checksum = calculateConfigChecksum(storedConfig, header.length);
    if (checksum == header.checksum) {
      if (header.length == sizeof(configData)) {
        memcpy(reinterpret_cast<uint8_t*>(&cfg), storedConfig, sizeof(configData));
        return cfg;
      }
      if (migrateConfigWithExpandedFirmwareVersion(storedConfig, header.length, cfg)) {
        DebugPrintln(3, "Migrated EEPROM config after firmware version field expansion");
        return cfg;
      }

      DebugPrintln(1, "Unsupported EEPROM config length; refusing header-backed config");
      return configData();
    } else {
      DebugPrintln(1, "EEPROM config checksum mismatch; refusing corrupted config");
      return configData();
    }
  } else {
    uint8_t rawConfig[sizeof(configData)] = {0};
    for (size_t i = 0; i < sizeof(rawConfig); ++i) {
      rawConfig[i] = EEPROM.read(cfgStart + i);
    }
    EEPROM.end();

    memcpy(reinterpret_cast<uint8_t*>(&cfg), rawConfig, sizeof(configData));
    if (cfg.valid == defconf.valid) {
      return cfg;
    }

    if (migrateConfigWithExpandedFirmwareVersion(rawConfig, sizeof(configData) - 1, cfg)) {
      DebugPrintln(3, "Migrated raw EEPROM config after firmware version field expansion");
      return cfg;
    }

    return cfg;
  }

  return configData();
}

bool hasEEPROMConfigHeader() {
  ConfigStorageGuard guard;
  if (!guard.acquired()) return false;
  ConfigStorageHeader header;
  EEPROM.begin(sizeEEPROM);
  const bool hasHeader = readConfigStorageHeader(header);
  EEPROM.end();
  return hasHeader;
}

bool saveConfigBackupToLittleFS(const configData &cfg) {
  configData backupCfg = cfg;
  backupCfg.password[0] = '\0';
  backupCfg.cpassword1[0] = '\0';
  backupCfg.cpassword2[0] = '\0';
  backupCfg.cpassword3[0] = '\0';
  backupCfg.spassword[0] = '\0';
  backupCfg.MdsApiKey[0] = '\0';
  backupCfg.mdsOtaSecret[0] = '\0';
  memset(backupCfg.nskey, 0, sizeof(backupCfg.nskey));
  memset(backupCfg.appkey, 0, sizeof(backupCfg.appkey));

  const ConfigBackupHeader header = {
    CONFIG_BACKUP_MAGIC,
    CONFIG_BACKUP_VERSION,
    uint16_t(sizeof(configData)),
    calculateConfigChecksum(reinterpret_cast<const uint8_t*>(&backupCfg), sizeof(configData))
  };

  if (LittleFS.exists(CONFIG_BACKUP_PATH)) {
    LittleFS.remove(CONFIG_BACKUP_PATH);
  }

  File backupFile = LittleFS.open(CONFIG_BACKUP_PATH, FILE_WRITE);
  if (!backupFile) {
    DebugPrintln(1, "Failed to open config backup file");
    return false;
  }

  const bool headerWritten = backupFile.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  const bool configWritten = backupFile.write(reinterpret_cast<const uint8_t*>(&backupCfg), sizeof(backupCfg)) == sizeof(backupCfg);
  backupFile.close();

  if (!headerWritten || !configWritten) {
    LittleFS.remove(CONFIG_BACKUP_PATH);
    DebugPrintln(1, "Failed to write config backup");
    return false;
  }

  DebugPrintln(3, "Config backup saved to LittleFS without secrets");
  return true;
}

bool restoreConfigBackupFromLittleFS(configData &cfg) {
  File backupFile = LittleFS.open(CONFIG_BACKUP_PATH, FILE_READ);
  ConfigBackupHeader header;
  if (!readConfigBackupHeader(backupFile, header)) {
    if (backupFile) {
      backupFile.close();
    }
    DebugPrintln(1, "Config backup missing or invalid");
    return false;
  }

  configData restoredCfg = configData();
  if (backupFile.read(reinterpret_cast<uint8_t*>(&restoredCfg), sizeof(restoredCfg)) != sizeof(restoredCfg)) {
    backupFile.close();
    DebugPrintln(1, "Failed to read config backup");
    return false;
  }
  backupFile.close();

  const uint32_t checksum = calculateConfigChecksum(reinterpret_cast<const uint8_t*>(&restoredCfg), sizeof(restoredCfg));
  if (checksum != header.checksum) {
    DebugPrintln(1, "Config backup checksum mismatch");
    return false;
  }

  cfg = restoredCfg;
  return true;
}

bool hasConfigBackupInLittleFS() {
  return LittleFS.exists(CONFIG_BACKUP_PATH);
}

String getFirmwareReleaseChannel() {
  return String(FIRMWARE_RELEASE_CHANNEL);
}

String getFirmwareReleaseLabel() {
  return String(FIRMWARE_RELEASE_LABEL);
}

String buildWebFilesVersionTag(const char *version, const char *channel) {
  if (version == nullptr || version[0] == '\0') {
    return "";
  }

  String versionTag = String(version);
  String releaseChannel = channel == nullptr || channel[0] == '\0'
    ? getFirmwareReleaseChannel()
    : String(channel);
  releaseChannel.trim();
  releaseChannel.toLowerCase();

  if (releaseChannel.length() == 0) {
    return versionTag;
  }

  return versionTag + "|" + releaseChannel;
}

bool saveWebFilesVersion(const char *version) {
  if (version == nullptr || version[0] == '\0') {
    return false;
  }

  if (LittleFS.exists(WEB_FILES_VERSION_PATH)) {
    LittleFS.remove(WEB_FILES_VERSION_PATH);
  }

  File versionFile = LittleFS.open(WEB_FILES_VERSION_PATH, FILE_WRITE);
  if (!versionFile) {
    DebugPrintln(1, "Failed to open web files version marker");
    return false;
  }

  versionFile.print(buildWebFilesVersionTag(version));
  versionFile.close();
  return true;
}

String getStoredWebFilesVersion() {
  if (!LittleFS.exists(WEB_FILES_VERSION_PATH)) {
    return "";
  }

  File versionFile = LittleFS.open(WEB_FILES_VERSION_PATH, FILE_READ);
  if (!versionFile) {
    return "";
  }

  String storedVersion = versionFile.readString();
  versionFile.close();
  storedVersion.trim();
  return storedVersion;
}

String getStoredWebFilesVersionOnly() {
  String storedVersion = getStoredWebFilesVersion();
  const int separatorIndex = storedVersion.indexOf('|');
  if (separatorIndex >= 0) {
    storedVersion = storedVersion.substring(0, separatorIndex);
  }
  storedVersion.trim();
  return storedVersion;
}

String getStoredWebFilesChannel() {
  String storedVersion = getStoredWebFilesVersion();
  const int separatorIndex = storedVersion.indexOf('|');
  if (separatorIndex < 0) {
    return "";
  }

  String storedChannel = storedVersion.substring(separatorIndex + 1);
  storedChannel.trim();
  storedChannel.toLowerCase();
  return storedChannel;
}

bool areWebFilesCurrent(const char *version, const char *channel) {
  if (version == nullptr || version[0] == '\0' || !LittleFS.exists(WEB_FILES_VERSION_PATH)) {
    return false;
  }

  String expectedTag = buildWebFilesVersionTag(version, channel);
  String storedVersion = getStoredWebFilesVersion();
  if (storedVersion == expectedTag) {
    return true;
  }

  // Legacy marker compatibility: allow plain version tags from older builds.
  return storedVersion == String(version) && getStoredWebFilesChannel().length() == 0;
}

String buildDefaultWebPassword() {
  return buildMacScopedDefaultSecret("BM-", ESP.getEfuseMac() & 0xFFFFFFULL, 6);
}

String buildDefaultApPassword() {
  return buildMacScopedDefaultSecret("AP-", ESP.getEfuseMac() & 0xFFFFFFFFULL, 8);
}

bool isGeneratedWebPassword(const char *value) {
  return hasExpectedGeneratedPrefix(value, "BM-", 9);
}

bool isGeneratedApPassword(const char *value) {
  return hasExpectedGeneratedPrefix(value, "AP-", 11);
}

bool isLegacyWeakWebPassword(const char *value) {
  return value == nullptr ||
         value[0] == '\0' ||
         strcmp(value, "boatmonitor") == 0 ||
         strcmp(value, "12345678") == 0 ||
         strcmp(value, "auto-generated") == 0;
}

bool isLegacyWeakApPassword(const char *value) {
  return value == nullptr ||
         value[0] == '\0' ||
         strcmp(value, "LoRaBoatMonitor") == 0 ||
         strcmp(value, "12345678") == 0 ||
         strcmp(value, "auto-generated") == 0;
}

bool normalizeMdsOtaEndpoint(const String &configuredValue, String &normalizedEndpoint) {
  String candidate = configuredValue;
  candidate.trim();

  if (candidate.length() == 0) {
    normalizedEndpoint = "";
    return false;
  }

  for (size_t i = 0; i < candidate.length(); i++) {
    const char c = candidate.charAt(i);
    if (c < 32 || c > 126) {
      normalizedEndpoint = "";
      return false;
    }
  }

  if (!candidate.startsWith("https://")) {
    normalizedEndpoint = "";
    return false;
  }

  const int queryStart = candidate.indexOf('?');
  if (queryStart >= 0) {
    candidate = candidate.substring(0, queryStart);
  }

  while (candidate.endsWith("/")) {
    candidate.remove(candidate.length() - 1);
  }

  const int lastSlash = candidate.lastIndexOf('/');
  if (lastSlash <= 8) {
    normalizedEndpoint = "";
    return false;
  }

  normalizedEndpoint = candidate;
  return true;
}

String buildMdsOtaMetadataUrl(const String &endpoint, const char *fileName) {
  String normalizedEndpoint;
  if (!normalizeMdsOtaEndpoint(endpoint, normalizedEndpoint) || fileName == nullptr || fileName[0] == '\0') {
    return "";
  }

  const int lastSlash = normalizedEndpoint.lastIndexOf('/');
  if (lastSlash <= 8) {
    return "";
  }

  return normalizedEndpoint.substring(0, lastSlash) + "/bin/" + String(fileName);
}

String buildMdsOtaWebBaseUrl(const String &endpoint) {
  String normalizedEndpoint;
  if (!normalizeMdsOtaEndpoint(endpoint, normalizedEndpoint)) {
    return "";
  }

  const int lastSlash = normalizedEndpoint.lastIndexOf('/');
  if (lastSlash <= 8) {
    return "";
  }

  return normalizedEndpoint.substring(0, lastSlash) + "/bin/web";
}

String normalizeSha256String(const String &sha256) {
  String normalized = sha256;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized.length() != 64) {
    return "";
  }

  for (size_t i = 0; i < normalized.length(); i++) {
    if (!isxdigit(static_cast<unsigned char>(normalized.charAt(i)))) {
      return "";
    }
  }

  return normalized;
}

String sha256ToHexString(const uint8_t digest[32]) {
  static const char hexChars[] = "0123456789abcdef";
  String hex;
  hex.reserve(64);
  for (size_t i = 0; i < 32; i++) {
    hex += hexChars[(digest[i] >> 4) & 0x0F];
    hex += hexChars[digest[i] & 0x0F];
  }
  return hex;
}

bool hasReasonableSystemTime() {
  const time_t now = time(nullptr);
  return now >= 1704067200; // 2024-01-01 00:00:00 UTC
}

float calculateBatteryVoltageFromAdc(const configData &cfg, uint16_t rawAdc) {
  return cfg.a2vslope * rawAdc * rawAdc + cfg.a1vslope * rawAdc + cfg.voffset;
}

bool sanitizeBatteryCalibration(configData &cfg, const configData &defaults) {
  const bool finiteCoefficients = isfinite(cfg.voffset) && isfinite(cfg.a1vslope) && isfinite(cfg.a2vslope);
  const float voltageAt0 = cfg.voffset;
  const float voltageAt512 = (cfg.a2vslope * 512.0f * 512.0f) + (cfg.a1vslope * 512.0f) + cfg.voffset;
  const float voltageAt1024 = (cfg.a2vslope * 1024.0f * 1024.0f) + (cfg.a1vslope * 1024.0f) + cfg.voffset;
  const bool plausibleCurve =
    finiteCoefficients &&
    cfg.a1vslope > 0.0f &&
    cfg.a1vslope < 0.20f &&
    cfg.a2vslope > -0.001f &&
    cfg.a2vslope < 0.001f &&
    cfg.voffset > -50.0f &&
    cfg.voffset < 50.0f &&
    isfinite(voltageAt0) &&
    isfinite(voltageAt512) &&
    isfinite(voltageAt1024) &&
    voltageAt0 > -5.0f &&
    voltageAt0 < 20.0f &&
    voltageAt512 > 0.0f &&
    voltageAt512 < 80.0f &&
    voltageAt1024 > 0.0f &&
    voltageAt1024 < 120.0f;

  if (plausibleCurve) {
    return false;
  }

  cfg.voffset = defaults.voffset;
  cfg.a1vslope = defaults.a1vslope;
  cfg.a2vslope = defaults.a2vslope;
  return true;
}

//**************************************************************************************
// Converting bool to int
int boolToInt(bool value){
  if(value == HIGH){
    return 1;
  }
  else{
    return 0;
  }
}

// Converting string to int
int toInteger(String settingValue){
   char intbuf[settingValue.length()+1];
   settingValue.toCharArray(intbuf, sizeof(intbuf));
   int f = atof(intbuf);
   return f;
 }

// Converting string to float
float toFloat(String settingValue){
  char floatbuf[settingValue.length()+1];
  settingValue.toCharArray(floatbuf, sizeof(floatbuf));
  float f = atof(floatbuf);
  return f;
}

// Converting string to long
long toLong(String settingValue){
  char longbuf[settingValue.length()+1];
  settingValue.toCharArray(longbuf, sizeof(longbuf));
  long l = atol(longbuf);
  return l;
}

// Converting String to integer and then to boolean
// 1 = true
// 0 = false
boolean toBoolean(String settingValue) {
  if(settingValue.toInt()==1){
    return true;
  } else {
    return false;
  }
}

// Convert string to char
char* toChar(String command){
  if(command.length()!=0){
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
  char* ch = new char[strlen("null") + 1];
  return ch;
}

// Convert Hex to Int
int HexToInt(char str[])
{
  return (int) strtol(str, 0, 16);
}

// Seaching the index for a value in a string array
int getindex(String data[], String compare){
  // For all array elements
  for(int i=0; data[i].length() != 0; i++){
    //DebugPrint(3, i);
    //DebugPrint(3, " : ");
    //DebugPrintln(3, data[i]);
    if(data[i] == compare){
      //DebugPrint(3, String(i));
      //DebugPrint(3, " : ");
      //DebugPrintln(3, String(data[i]));
      return i; 
    }
  }
  return 0;
}

// Displays a WLAN field strength symbol with characters
// 0))))
String wlansymbol(){
  String symbol = "O";
  float fieldstr = float(WiFi.RSSI());
  if(fieldstr > 0){
    fieldstr = -100.0;
  }
  float wlqual = 100  - (((fieldstr * -1) - 50) * 2);
  if(wlqual < 0){
    wlqual = 0;
  }
  if(wlqual > 100){
    wlqual = 100;
  }
  if(wlqual > 20){symbol = "0)";}
  if(wlqual > 40){symbol = "0))";}
  if(wlqual > 60){symbol = "0)))";}
  if(wlqual > 80){symbol = "0))))";}
  return symbol;
}

// Displays a WLAN field strength quality in %
int wlanquality(){
  float fieldstr = float(WiFi.RSSI());
  if(fieldstr > 0){
    fieldstr = -100.0;
  }
  int wlqual = 100  - (((fieldstr * -1) - 50) * 2);
  if(wlqual < 0){
    wlqual = 0;
  }
  if(wlqual > 100){
    wlqual = 100;
  }
  return wlqual;
}

// Truncate a float number 23.234 -> 23.2
float truncate1(float value){
  float result = roundf(value * 10) / 10;
  return result;
}

// Truncate a float number 23.234 -> 23.23
float truncate2(float value){
  float result = roundf(value * 100) / 100;
  return result;
}

// First zero character for integer lower 10
// value range from 0...99
String firstzero(int value){
  String output;
  if(value < 0){
    value = 0;
  }
  if(value > 99){
    value = 99;
  }
  if(value < 10){
    output = "0" + String(value);
  }
  else{
    output = String(value);
  }
  return output;
}

static void cooperativeDelay(uint32_t ms) {
  const unsigned long start = millis();
  while (millis() - start < ms) {
    delay(1);
    yield();
  }
}

//**************************************************************************************
// Clear serial 1 RX buffer
void Serial1Clear(){
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
} 

// Clear serial 2 RX buffer
void Serial2Clear(){
  while(Serial2.available() > 0) {
    char t = Serial2.read();
  }
} 

// Convert float to int 48.2345678 -> 4823
// Only usable for float 0.000...655.35
uint16_t float2int(float value){
  float result = roundf(value * 100);
  if(result < 0){
    result = 0;
  }
  if(result > 65535){
    result = 65535;
  }
  return result;
}

// Convert float to int 48.2345678 -> 48234
// Only usable for float 0.000...65.535
uint16_t float3int(float value){
  float result = roundf(value * 1000);
  if(result < 0){
    result = 0;
  }
  if(result > 65535){
    result = 65535;
  }
  return result;
}

// Convert float to int 3.2345678 -> 323456
// Only usable for float 0.000...6.5535
uint16_t float4int(float value){
  float result = roundf(value * 10000);
  if(result < 0){
    result = 0;
  }
  if(result > 65535){
    result = 65535;
  }
  return result;
}

// Flash LED for x ms
void flashLED(int duration){
 digitalWrite(ledPin, HIGH);   // On (High activ)
 //delay(duration);
 vTaskDelay(duration);
 digitalWrite(ledPin, LOW);    // Off
}

// Dewpoint calculation
// Refer: https://de.wikipedia.org/wiki/Taupunkt
float dewpoint(float temp, float humidity) {
  float K1 = 6.112;   // [hPa]
  float K2 = 17.62;   // [1]
  float K3 = 243.12;  // [°C]
  float term1 = (K2 * temp) / (K3 + temp);
  float term2 = (K2 * K3) / (K3 + temp);
  float term3 = log(humidity / 100);                  // Humidiy range 0...1
  float dewp = K3 *((term1 + term3)/(term2 - term3));
  return dewp;
}

// Checksum calculation over binary array
byte BinCheckSum(byte *Data[]) {
  byte checksum = 0;
  // Iterate over the string, ADD each byte with the total sum
  for (int c = 0; c < sizeof(*Data); c++) {
    checksum += *Data[c];
  } 
  // Return the result
  return checksum;
}

// Checksum calculation for NMEA
char CheckSum(String NMEAData) {
  char checksum = 0;
  // Iterate over the string, XOR each byte with the total sum
  for (int c = 0; c < NMEAData.length(); c++) {
    checksum = char(checksum ^ NMEAData.charAt(c));
  } 
  // Return the result
  return checksum;
}

// Check NMEA string
bool CheckNMEA(String NMEAstring) {
  bool check = false;
  int i1 = NMEAstring.indexOf( '$');
  int i2 = NMEAstring.indexOf( '*');
  int i3 = NMEAstring.length();
  String NMEApartial = NMEAstring.substring(i1+1, i2);
  String cksum1 = NMEAstring.substring(i2+1, i3);
  String cksum2 = String(CheckSum(NMEApartial), HEX);
  int cksum3 = HexToInt(toChar(cksum1));
  int cksum4 = HexToInt(toChar(cksum2));
//  DebugPrintln(3, cksum1);
//  DebugPrintln(3, cksum2);
//  DebugPrintln(3, cksum3);
//  DebugPrintln(3, cksum4);
  if(cksum3 == cksum4){
    check = true;
  }
  else{
    check = false;
  }
  return check;
}

// Read and print sensor values
void readValues(configData myactconf) {
    boolean debugBME280 = false;
    boolean debugVEdirect = false;
    boolean debugADC = false;
    boolean debugAlarm1 = false;
    boolean debugRelay = false;
    boolean debugEnvSensor = false;
    boolean debugTemp1wire = false;
    // Is connected with extern WLAN network
    if(WiFi.localIP().toString() != "0.0.0.0"){
      fieldstrength = float(WiFi.RSSI());
      if(fieldstrength > 0){
        fieldstrength = -100.0;
      }
      quality = 100  - (((fieldstrength * -1) - 50) * 2);
      if(quality < 0){
        quality = 0;
      }
      if(quality > 100){
        quality = 100;
      }
    }
    else{
      fieldstrength = 0;
      quality = 0;
    }
    
    // Read BME280 sensor values
    if (String(myactconf.envSensor) == "BME280") {
      if (debugBME280) {
        DebugPrint(3, "Temperature = ");
      }
      const float temperatureC = bme.readTemperature();
      if(String(actconf.tempUnit) == "C"){
        temperature = temperatureC;
        if (debugBME280) {
          DebugPrint(3, temperature);
          DebugPrintln(3, " *C");
        }
      }
      else{
        temperature = temperatureC * 9 / 5 + 32;
        if (debugBME280) {
          DebugPrint(3, temperature);
          DebugPrintln(3, " *F");
        }
      }
  
      if (debugBME280) {
        DebugPrint(3, "Pressure = ");
      }
      pressure = bme.readPressure() / 100.0F;
      if (debugBME280) {
        DebugPrint(3, pressure);
        DebugPrintln(3, " hPa");
      }
  
      if (debugBME280) {
        DebugPrint(3, "Approx. Altitude = ");
      }
      altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
      if (debugBME280) {
        DebugPrint(3, altitude);
        DebugPrintln(3, " m");
      }
  
      if (debugBME280) {
        DebugPrint(3, "Humidity = ");
      }
      humidity = bme.readHumidity();
      if (debugBME280) {
        DebugPrint(3, humidity);
        DebugPrintln(3, " %");
      }
  
      if (debugBME280) {
        DebugPrint(3, "Dewpoint = ");
      }
      dewp = dewpoint(temperatureC, humidity);
      if(String(actconf.tempUnit) == "C"){
        if (debugBME280) {
          DebugPrint(3, dewp);
          DebugPrintln(3, " *C");
        }
      }
      else{
        dewp = dewp * 9 / 5 + 32;
        if (debugBME280) {
          DebugPrint(3, dewp);
          DebugPrintln(3, " *F");
        }
      }
    }

    // Show and copy BMV-712 battery monitor values
    if (String(myactconf.envSensor) == "VEdirect-Read") {
      if (debugVEdirect) {
        DebugPrint(3, "VE.direct Voltage = ");
        DebugPrint(3, vedirectVoltage);
        DebugPrintln(3, " V");

        DebugPrint(3, "VE.direct Current = ");
        DebugPrint(3, vedirectCurrent);
        DebugPrintln(3, " A");

        DebugPrint(3, "VE.direct Temperature = ");
        DebugPrint(3, vedirectTemp);
        DebugPrintln(3, " *C");
      }

      //Copy voltage and temperature in in original values
      voltage = vedirectVoltage;
      temp1wire = vedirectTemp;
    }
    else{
      vedirectVoltage = 0.0;
      vedirectCurrent= 0.0;
      vedirectTemp = 0.0;
    }
    
    // Analog input 0...3.3V => 0...33V => 0...4096
    voltageadc = 0;
    if (String(myactconf.envSensor) != "VEdirect-Read") {
      const int analogVoltageRaw = analogRead(ANALOG_IN);
      voltageadc = analogVoltageRaw;
      if (debugVEdirect) {
        DebugPrint(3, "Voltage = ");
      }
      voltage = calculateBatteryVoltageFromAdc(myactconf, analogVoltageRaw);
      if (debugVEdirect) {
        DebugPrint(3, analogVoltageRaw);
        DebugPrintln(3, " dig");
      }
    }
    if (debugVEdirect) {
      DebugPrint(3, "Voltage = ");
      DebugPrint(3, voltage);
      DebugPrintln(3, " V");
    }
    // Battery Capacity 100% = 12,70V, 0% = 10,50V Pb-Accu
    capacity = (voltage * 100 / 2.2) - 477.27;
    if(capacity < 0){
      capacity = 0;
    }
    if(capacity > 100){
      capacity = 100;
    }
    if (debugVEdirect) {
      DebugPrint(3, "Capacity = ");
      DebugPrint(3, capacity);
      DebugPrintln(3, " %");
    }

    if (debugADC) {
      DebugPrint(3, "Tank1 = ");
    }
    // Analog input 0...3.3V => 0...33V => 0...4096
    const int rawTank1 = analogRead(TANK1_IN);
    tank1 = 3.3 / 4096 * rawTank1;
    tank1adc = rawTank1;  // real adc value
    uint16_t sensorMin = 0;
    uint16_t sensorMax = 3674;  // max value without Resistor.
    tank1adc = map(tank1adc, sensorMin, sensorMax, 0, 4096);

    if (debugADC) {
      DebugPrint(3, tank1);
      DebugPrint(3, " V ");
    }
    tank1p = (myactconf.a2t1slope * tank1 * tank1) + (myactconf.a1t1slope * tank1) + myactconf.t1offset;
  	// Limiting
  	if(tank1p > 100){
      tank1p = 100;
    }
    if(tank1p < 0){
      tank1p = 0;
    }
    if (debugADC) {
      DebugPrint(3, tank1p);
      DebugPrintln(3, " %");
    }

    if (debugADC) {
      DebugPrint(3, "Tank2 = ");
    }
    // Analog input 0...3.3V => 0...33V => 0...4096
    const int rawTank2 = analogRead(TANK2_IN);
    tank2 = 3.3 / 4096 * rawTank2;
    tank2adc = rawTank2;  // real adc value
    // apply the calibration to the sensor reading
    //uint16_t sensor2Min = 0;
    //uint16_t sensor2Max = 3674;
    //tank2adc = map(tank2adc, sensor2Min, sensor2Max, 0, 4096);

    // in case the sensor value is outside the range seen during calibration
    tank2adc = constrain(tank2adc, 0, 4096);


    if (debugADC) {
      DebugPrint(3, tank2);
      DebugPrint(3, " V ");
    }
    tank2p = (myactconf.a2t2slope * tank2 * tank2) + (myactconf.a1t2slope * tank2) + myactconf.t2offset;
  	// Limiting
  	if(tank2p > 100){
      tank2p = 100;
    }
    if(tank2p < 0){
      tank2p = 0;
    }
    if (debugADC) {
      DebugPrint(3, tank2p);
      DebugPrintln(3, " %");
    }

    // Digital input 12V activ via opto coupler
    alarm1 = !digitalRead(alarmPin);  // Invert for easer logic.
    if (debugAlarm1) {
      DebugPrint(3, "Alarm = ");
      DebugPrint(3, alarm1);
      DebugPrintln(3, "  ");
    }

    if (debugRelay) {
      DebugPrint(3, "Relay = ");
      // Digital relay output high activ 
      DebugPrint(3, myactconf.relay);
      DebugPrintln(3, "  ");
    }

    if (debugEnvSensor) {
      DebugPrint(3, "envSensor = ");
      DebugPrint(3, String(myactconf.envSensor));
      DebugPrintln(3, "  ");
    }

    // Read 1Wire sensor values for battery temperature
    if (String(myactconf.tempSensorType) == "DS18B20") {
      sensors.requestTemperatures();            // Send the command to get temperatures
      temp1wire = sensors.getTempCByIndex(0);   // Read 1Wire sensor 0
      // Error correction for wrong 1Wire values (-127)
      if(temp1wire < float(-50)){
        temp1wire = temp1wireold;
      }
      else{
        temp1wireold = temp1wire;
      }
    }
    // If sensor disabled
    else{
      temp1wire = -99.9;
    }
    // Unit selection
    if (debugTemp1wire) {
      DebugPrint(3, "BattTemp = ");
    }
    if(String(myactconf.tempUnit) == "C"){
      if (debugTemp1wire) {
        DebugPrint(3, temp1wire);
        DebugPrintln(3, " *C");
      }
    }
    else{
      temp1wire = temp1wire * 9 / 5 + 32;
      if (debugTemp1wire) {
        DebugPrint(3, temp1wire);
        DebugPrintln(3, " *F");
      }
    }
    //DebugPrintln(3, "");
}

// Display sensor values on OLED
void writeDisplay() {
  // Formating display data
  char cnt[10] = "";
  //dtostrf(int(counter16), 5, 0, cnt);
  //dtostrf(int(LMIC.seqnoUp), 5, 0, cnt);
  char tmp[10];      
  dtostrf(temperature, 5, 1, tmp);
  char pres[10];
  dtostrf(pressure, 5, 0, pres);
  char hum[10];
  dtostrf(humidity, 5, 1, hum);
  char dew[10];
  dtostrf(dewp, 5, 1, dew);
  char dt[10];
  dtostrf(float(TX_INTERVAL), 4, 0, dt);
  char vol[10];
  dtostrf(voltage, 5, 1, vol);
  char tmp2[10];
  dtostrf(temp1wire, 5, 1, tmp2);
  char lat[10];
  dtostrf(latitude, 5, 3, lat);
  char lon[10];
  dtostrf(longitude, 6, 4, lon);
  char tnk1[10];
  dtostrf(tank1p, 5, 1, tnk1);
  char tnk2[10];
  dtostrf(tank2p, 5, 1, tnk2);
  char alm[10];
  dtostrf(int(alarm1), 5, 0, alm);
  char rel[10];
  dtostrf(int(actconf.relay), 5, 0, rel);
  
  // Refresh OLED data
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0,0,"LoRa Monitor");
  //u8x8.drawString(0,0,"NoWa(C)OBP");
  //u8x8.drawString(11,0,actconf.fversion);
  u8x8.drawString(0,1,"C:");
  u8x8.drawString(2,1,cnt);
  u8x8.drawString(0,2,"T:");
  u8x8.drawString(2,2,tmp);
  u8x8.drawString(0,3,"P:");
  u8x8.drawString(2,3,pres);
  u8x8.drawString(0,4,"H:");
  u8x8.drawString(2,4,hum);
  u8x8.drawString(0,5,"D:");
  u8x8.drawString(2,5, dew);
  u8x8.drawString(8,1,"dT:");
  u8x8.drawString(11,1, dt);
  u8x8.drawString(15,1, "s");
  u8x8.drawString(9,2,"V:");
  u8x8.drawString(11,2, vol);
  u8x8.drawString(8,3,"T2:");
  u8x8.drawString(11,3, tmp2);
  u8x8.drawString(8,4,"Y:");
  u8x8.drawString(10,4, lat);
  u8x8.drawString(8,5,"X:");
  u8x8.drawString(10,5, lon);
  u8x8.drawString(0,6,"L:");
  u8x8.drawString(2,6, tnk1);
  u8x8.drawString(8,6,"L2:");
  u8x8.drawString(11,6, tnk2);
  u8x8.drawString(0,7,"A:");
  u8x8.drawString(2,7, alm);
  u8x8.drawString(8,7,"R:");
  u8x8.drawString(11,7, rel);
  u8x8.refreshDisplay();    // Only required for SSD1606/7 
}

// Display sensor values on OLED
void writeDisplayValues(configData myactconf) {
  //WebSerial.println("Update Display.");
  // Formating display data
  char cnt[10] = "";
  //dtostrf(int(counter16), 5, 0, cnt);
  //dtostrf(int(LMIC.seqnoUp), 5, 0, cnt);
  char tmp[10];      
  dtostrf(temperature, 5, 1, tmp);
  char pres[10];
  dtostrf(pressure, 5, 0, pres);
  char hum[10];
  dtostrf(humidity, 5, 1, hum);
  char dew[10];
  dtostrf(dewp, 5, 1, dew);
  char dt[10];
  dtostrf(float(TX_INTERVAL), 4, 0, dt);
  char vol[10];
  dtostrf(voltage, 5, 1, vol);
  char tmp2[10];
  dtostrf(temp1wire, 5, 1, tmp2);
  char lat[10];
  dtostrf(latitude, 5, 3, lat);
  char lon[10];
  dtostrf(longitude, 6, 4, lon);
  char tnk1[10];
  dtostrf(tank1p, 5, 1, tnk1);
  char tnk2[10];
  dtostrf(tank2p, 5, 1, tnk2);
  char alm[10];
  dtostrf(int(alarm1), 5, 0, alm);
  char rel[10];
  dtostrf(int(myactconf.relay), 5, 0, rel);
  
  // Refresh OLED data
  //u8x8.setFont(u8x8_font_chroma48medium8_r);
  //u8x8.drawString(0,0,"NoWa(C)OBP");
  //u8x8.drawString(11,0,actconf.fversion);
  //u8x8.drawString(0,1,"C:");
  u8x8.drawString(2,1,cnt);
  //u8x8.drawString(0,2,"T:");
  u8x8.drawString(2,2,tmp);
  //u8x8.drawString(0,3,"P:");
  u8x8.drawString(2,3,pres);
  //u8x8.drawString(0,4,"H:");
  u8x8.drawString(2,4,hum);
  //u8x8.drawString(0,5,"D:");
  u8x8.drawString(2,5, dew);
  //u8x8.drawString(8,1,"dT:");
  u8x8.drawString(11,1, dt);
  //u8x8.drawString(15,1, "s");
  //u8x8.drawString(9,2,"V:");
  u8x8.drawString(11,2, vol);
  //u8x8.drawString(8,3,"T2:");
  u8x8.drawString(11,3, tmp2);
  //u8x8.drawString(8,4,"Y:");
  u8x8.drawString(10,4, lat);
  //u8x8.drawString(8,5,"X:");
  u8x8.drawString(10,5, lon);
  //u8x8.drawString(0,6,"L:");
  u8x8.drawString(2,6, tnk1);
  //u8x8.drawString(8,6,"L2:");
  u8x8.drawString(11,6, tnk2);
  //u8x8.drawString(0,7,"A:");
  u8x8.drawString(2,7, alm);
  //u8x8.drawString(8,7,"R:");
  u8x8.drawString(11,7, rel);
  u8x8.refreshDisplay();    // Only required for SSD1606/7
}

void writeDisplayByMode(configData myactconf) {
  if (String(myactconf.OledDisplayMode) == "Status") {
    const String wifiState = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "not connected";
    const String mdsState = String(myactconf.SendDataViaWifi) == "Yes" ? "MDS enabled" : "MDS disabled";
    const String loraMode = "LoRa " + String(myactconf.loraOperationMode);
    const String priority = "Prio " + String(myactconf.transmitPriority);
    const String standbyLevel = digitalRead(alarmPin) == LOW ? "GPIO39 LOW" : "GPIO39 HIGH";
    const String batteryLine = "Bat " + String(voltage, 2) + "V " + String(capacity, 0) + "%";
    const String webLine = "Web " + String(myactconf.fversion);
    writeDisplayStatusScreen("Device status",
                             wifiState,
                             mdsState,
                             loraMode,
                             priority,
                             standbyLevel,
                             batteryLine,
                             webLine);
    return;
  }

  writeDisplay();
}

namespace {
String sanitizeDisplayText(const String &value) {
  String sanitized;
  sanitized.reserve(value.length());

  for (size_t i = 0; i < value.length(); i++) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c >= 32 && c <= 126) {
      sanitized += char(c);
    } else if (c == '\n' || c == '\r' || c == '\t') {
      sanitized += ' ';
    } else {
      sanitized += '?';
    }
  }

  while (sanitized.indexOf("  ") >= 0) {
    sanitized.replace("  ", " ");
  }

  sanitized.trim();
  return sanitized;
}

String fitDisplayLine(const String &value) {
  String line = sanitizeDisplayText(value);
  if (line.length() > 16) {
    line = line.substring(0, 16);
  }
  return line;
}

String lastDisplayLines[8];
unsigned long lastDisplayProgressRefreshMillis = 0;
int lastDisplayProgressPercent = -1;
String lastDisplayProgressTitle = "";
String lastDisplayProgressMessage = "";
String lastDisplayProgressDetail = "";
String lastDisplayProgressFooter = "";

void resetDisplayProgressCache() {
  lastDisplayProgressRefreshMillis = 0;
  lastDisplayProgressPercent = -1;
  lastDisplayProgressTitle = "";
  lastDisplayProgressMessage = "";
  lastDisplayProgressDetail = "";
  lastDisplayProgressFooter = "";
}
}

void writeDisplayStatusScreen(const String &title,
                             const String &line1,
                             const String &line2,
                             const String &line3,
                             const String &line4,
                             const String &line5,
                             const String &line6,
                             const String &line7) {
  const String lines[] = {
    fitDisplayLine(title),
    fitDisplayLine(line1),
    fitDisplayLine(line2),
    fitDisplayLine(line3),
    fitDisplayLine(line4),
    fitDisplayLine(line5),
    fitDisplayLine(line6),
    fitDisplayLine(line7)
  };

  bool displayChanged = false;
  for (uint8_t index = 0; index < 8; index++) {
    if (lastDisplayLines[index] != lines[index]) {
      displayChanged = true;
      break;
    }
  }

  if (!displayChanged) {
    return;
  }

  u8x8.setPowerSave(0);
  u8x8.clearDisplay();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  for (uint8_t index = 0; index < 8; index++) {
    if (lines[index].length() > 0) {
      u8x8.drawString(0, index, lines[index].c_str());
    }
    lastDisplayLines[index] = lines[index];
  }
  u8x8.refreshDisplay();
}

void writeDisplayProgressScreen(const String &title,
                               const String &message,
                               size_t current,
                               size_t total,
                               const String &detail,
                               const String &footer) {
  String percentLine = "Progress";
  String countLine = "";
  int percent = -1;
  if (total > 0) {
    percent = int((current * 100U) / total);
    percentLine = String(percent) + "%";
    countLine = String(current) + "/" + String(total);
  } else if (current > 0) {
    countLine = String(current) + " bytes";
  }

  const String normalizedTitle = fitDisplayLine(title);
  const String normalizedMessage = fitDisplayLine(message);
  const String normalizedDetail = fitDisplayLine(detail);
  const String normalizedFooter = fitDisplayLine(footer);
  const unsigned long now = millis();
  const bool contentChanged = normalizedTitle != lastDisplayProgressTitle ||
                              normalizedMessage != lastDisplayProgressMessage ||
                              normalizedDetail != lastDisplayProgressDetail ||
                              normalizedFooter != lastDisplayProgressFooter;
  const bool percentChanged = percent != lastDisplayProgressPercent;
  const int previousBucket = lastDisplayProgressPercent >= 0 ? (lastDisplayProgressPercent / 20) : -1;
  const int currentBucket = percent >= 0 ? (percent / 20) : -1;
  const bool refreshWindowElapsed = now - lastDisplayProgressRefreshMillis >= 1500;
  const bool shouldRefresh = (contentChanged && refreshWindowElapsed) ||
                             (percent >= 0 && (percent == 0 || percent == 100 || (percentChanged && currentBucket != previousBucket))) ||
                             refreshWindowElapsed;

  if (!shouldRefresh) {
    return;
  }

  lastDisplayProgressTitle = normalizedTitle;
  lastDisplayProgressMessage = normalizedMessage;
  lastDisplayProgressDetail = normalizedDetail;
  lastDisplayProgressFooter = normalizedFooter;
  lastDisplayProgressPercent = percent;
  lastDisplayProgressRefreshMillis = now;

  writeDisplayStatusScreen(title,
                           normalizedMessage,
                           countLine,
                           percentLine,
                           normalizedDetail,
                           normalizedFooter);
}

// Parse a bounded amount of GPS input without blocking LoRa or web tasks.
void readGPSValues() {
  boolean debugGPS = false;
  rmc_finish = false;
  size_t processedBytes = 0;
  constexpr size_t MAX_GPS_BYTES_PER_LOOP = 256;
  constexpr size_t MAX_NMEA_SENTENCE_LENGTH = 160;
  while(Serial2.available() && !rmc_finish && !lora_activ && !loraEvent_activ && processedBytes < MAX_GPS_BYTES_PER_LOOP) {
    inByte = Serial2.read(); // Read data
    processedBytes++;
    // Start by $ or ! string
    if ((start==0) && ((inByte == '$')||(inByte == '!'))) {
      start = 1;
      c_counter = 0;          // Clear commata counter
      nmea = "";
      nmea_all = "";
    }
    if(start==1 && (inByte==44)) {c_counter++;}   // Count all commata
    if (start == 1) {
      if (nmea_all.length() >= MAX_NMEA_SENTENCE_LENGTH) {
        start = 0;
        nmea_all = "";
        c_counter = 0;
        continue;
      }
      nmea_all.concat((char)inByte);
    }
    if((inByte==13) && (start==1)) { // Detect end of NMEA0183 telegram (CR)
      start=0;
      if (debugGPS) {
        DebugPrintln(3, nmea_all); // Print all NMEA0183 telegrams
      }
      if (nmea_all.substring(3,6) == "RMC") {
        nmea = nmea_all;            // Take over RMC telegram
        if (debugGPS) {
          DebugPrint(3, "Commata: ");
          DebugPrintln(3, c_counter); // Print commata counter for RMC
        }
        // If NMEA cecksum ok
        if(CheckNMEA(nmea) && c_counter == 12) {
          if (debugGPS) {
            DebugPrintln(3, "RMC message processed"); // Only print RMC telegram
            DebugPrint(3, "Sta ");
          }
          gpsStatus = getRMC_status(nmea);
          if (debugGPS) {
            DebugPrintln(3, gpsStatus);
            DebugPrint(3, "Lat ");
          }
          latitude = getRMC_LatDec(nmea);
          if (debugGPS) {
            DebugPrint(3, String(latitude, 6));
          }
          latitudeNS = getRMC_LatNS(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(latitudeNS));         
            DebugPrint(3, "Lon ");
          }
          longitude = getRMC_LonDec(nmea);
          if (debugGPS) {
            DebugPrint(3, String(longitude, 6));
          }
          longitudeEW = getRMC_LonEW(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(longitudeEW));
            DebugPrintln(3, "");
          }

          if (debugGPS) {
            DebugPrint(3, "Time ");
          }
          hour = getRMC_hour(nmea);
          if (debugGPS) {
            DebugPrint(3, String(hour));
            DebugPrint(3, ":");
          }
          minute = getRMC_min(nmea);
          if (debugGPS) {
            DebugPrint(3, String(minute));
            DebugPrint(3, ":");
          }
          second = getRMC_sec(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(second));
            DebugPrintln(3, "");
          }

          if (debugGPS) {
            DebugPrint(3, "Date ");
          }
          day = getRMC_Day(nmea);
          if (debugGPS) {
            DebugPrint(3, String(day));
            DebugPrint(3, ".");
          }
          month = getRMC_Month(nmea);
          if (debugGPS) {
            DebugPrint(3, String(month));
            DebugPrint(3, ".");
          }
          year = getRMC_Year(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(year));
            DebugPrintln(3, "");
          }

          if (debugGPS) {
            DebugPrint(3, "Speed ");
          }
          gpsspeed = getRMC_Speed(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(gpsspeed));
            DebugPrint(3, "Course ");
          }
          course = getRMC_Course(nmea);
          if (debugGPS) {
            DebugPrintln(3, String(course));
            DebugPrintln(3, "");
          }

          if (debugGPS) {
            DebugPrint(3, "Env Sensor ");
            DebugPrintln(3, String(actconf.envSensor));
            DebugPrintln(3, "");
          }

          rmc_finish = true;
        }
        else{
          DebugPrintln(3, "Checksum wrong!");
          nmea = "";
          nmea_all = "";
        }
      }
      else{
        nmea = "";
        nmea_all = "";
      }
    }
  }
}

// Timer2 Interrupt relay timer
void relayTimerInterrupt(){
  relaytimer--; // Decrement relay timer
  if(relaytimer <= 0){
    relaytimer = 0;
    digitalWrite(relayPin, LOW); // Relay off
    actconf.relay = 0;
    //saveEEPROMConfig(actconf);
  }
  else{
    digitalWrite(relayPin, HIGH); // Relay on
  }
}

// Timer3 routine for NMEA data sending with normal speed (all 2s)
void sendNMEA() {
  // Set data sending flag1
  //Serial.println("NMEA timer.");
  flag1 = true;
}

// handles uploads
const size_t MAX_FILE_MANAGER_UPLOAD_SIZE = 262144;

static bool isSafeUploadFilename(const String &filename) {
  if (filename.length() == 0) {
    return false;
  }
  if (filename.startsWith("/") || filename.startsWith(".")) {
    return false;
  }
  if (filename.indexOf("..") >= 0) {
    return false;
  }
  if (filename.indexOf('\\') >= 0) {
    return false;
  }
  if (filename.endsWith("/")) {
    return false;
  }

  const char *allowedExtensions[] = {
    ".css",
    ".gz",
    ".htm",
    ".html",
    ".ico",
    ".js",
    ".json",
    ".txt"
  };
  for (const char *extension : allowedExtensions) {
    if (filename.endsWith(extension)) {
      return true;
    }
  }

  return false;
}

static String uploadTargetPath(const String &filename) {
  return "/" + filename;
}

static String uploadTempPath(const String &filename) {
  return uploadTargetPath(filename) + ".upload";
}

static String uploadBackupPath(const String &filename) {
  return uploadTargetPath(filename) + ".rollback";
}

static void cleanupUpload(AsyncWebServerRequest *request, const String &filename) {
  if (request->_tempFile) {
    request->_tempFile.close();
  }
  LittleFS.remove(uploadTempPath(filename));
}

bool handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);

  if (!isSafeUploadFilename(filename)) {
    Serial.println("Upload rejected: unsafe filename");
    return false;
  }

  if (index + len > MAX_FILE_MANAGER_UPLOAD_SIZE) {
    Serial.println("Upload rejected: file too large");
    cleanupUpload(request, filename);
    return false;
  }

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    const String tempPath = uploadTempPath(filename);
    const String targetPath = uploadTargetPath(filename);
    const String backupPath = uploadBackupPath(filename);
    if (LittleFS.exists(backupPath)) {
      if (!LittleFS.exists(targetPath)) {
        LittleFS.rename(backupPath, targetPath);
      } else {
        LittleFS.remove(backupPath);
      }
    }
    if (LittleFS.exists(tempPath)) {
      LittleFS.remove(tempPath);
    }
    request->_tempFile = LittleFS.open(tempPath, "w");
    if (!request->_tempFile) {
      Serial.println("Upload rejected: temporary file could not be opened");
      return false;
    }
    Serial.println(logmessage);
  }

  if (len) {
    // stream the incoming chunk to the opened file
    if (!request->_tempFile || request->_tempFile.write(data, len) != len) {
      Serial.println("Upload rejected: write failed");
      cleanupUpload(request, filename);
      return false;
    }
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
    Serial.println(logmessage);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    const String tempPath = uploadTempPath(filename);
    const String targetPath = uploadTargetPath(filename);
    const String backupPath = uploadBackupPath(filename);
    LittleFS.remove(backupPath);

    const bool hadOriginal = LittleFS.exists(targetPath);
    if (hadOriginal && !LittleFS.rename(targetPath, backupPath)) {
      LittleFS.remove(tempPath);
      Serial.println("Upload rejected: existing file could not be backed up");
      return false;
    }

    if (!LittleFS.rename(tempPath, targetPath)) {
      LittleFS.remove(tempPath);
      if (hadOriginal) {
        LittleFS.rename(backupPath, targetPath);
      }
      Serial.println("Upload rejected: install failed");
      return false;
    }
    LittleFS.remove(backupPath);
    Serial.println(logmessage);
  }
  return true;
}

// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

String getheader(configData myactconf) {
  String content = readFile2(LittleFS, "/header.html");
  //content.replace("%header%", String(readFile2(LittleFS, "/header.html")));
  content.replace("%devname%", htmlEscapeLocal(String(myactconf.devname)));
  content.replace("%crights%", htmlEscapeLocal(String(myactconf.crights)));
  content.replace("%fversion%", htmlEscapeLocal(String(myactconf.fversion)));
  content.replace("%quality%", String(int(quality)));
  return content;
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
