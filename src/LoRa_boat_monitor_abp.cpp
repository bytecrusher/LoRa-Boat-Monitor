 /*******************************************************************************
 * Copyright (c) 2023 Norbert Walter modified by Guntmar Hoeche
 * 
 * License: GNU GPL V3
 * https://www.gnu.org/licenses/gpl-3.0.txt
 *
 * LoRa_Boat_Monitor_abp.cpp
 * 
 * Based on work of 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * This uses ABP (Activation-by-personalisation), where a DevAddr and
 * Session keys are preconfigured (unlike OTAA, where a DevEUI and
 * application key is configured, while the DevAddr and session keys are
 * assigned/generated in the over-the-air-activation procedure).
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!
 *
 * To use this sketch, first register your application and device with
 * the things network, to set or generate a DevAddr, NwkSKey and
 * AppSKey. Each device should have their own unique values for these
 * fields.
 *
 *******************************************************************************
 *
 * Attention! Check for EU868 the build settings in plarformio.ini
 *     -D ARDUINO_LMIC_PROJECT_CONFIG_H_SUPPRESS
 *     -D CFG_eu868=1
 *     -D CFG_sx1276_radio=1
 *
 * Set the #define CFG_eu868=1
 * 
 *******************************************************************************/
 
// Includes
#include <Arduino.h>            // Arduino Environment
#include <WiFi.h>               // WiFi lib with TCP server and client
#include <WiFiClient.h>         // WiFi lib for clients
#include <AsyncTCP.h>           // asynchron TCP lib
#include <WebSerial.h>          // Serial loggin as a webpage
#include <ESPmDNS.h>            // mDNS lib
#include <Update.h>             // Web Update server
#include "driver/adc.h"         // adc lib
#include <ArduinoJson.h>        // JSON lib
#include <U8x8lib.h>            // OLED Lib
#include <arduino_lmic.h>       // LoRa Lib, previous lmic.h
#include <hal/hal.h>            // LoRa Lib
#include <Wire.h>               // Wire Lib
#include <SPI.h>                // SPI/I2C Lib for OLED and BME280
#include <Adafruit_Sensor.h>    // BME280
#include <Adafruit_BME280.h>    // BME280
#include <Ticker.h>             // Ticker lib
#include <EEPROM.h>             // EEPROM lib
#include <WString.h>            // Needs for structures
#include <OneWire.h>            // 1Wire lib
#include <DallasTemperature.h>  // DS18B20 lib
#include "driver/rtc_io.h"      // rtc lib
#include "esp_system.h"         // reset reason
#include "FS.h"                 // FS lib
#include <LittleFS.h>           // Lib for LittleFS filesystem
#include <time.h>               // Time lib
#include <new>
#include <stddef.h>
#include "func_webclient.h"     // my lib for webclient connection (getting files for webserver)
#include <stdint.h>
#include "Configuration.h"      // Configuration
#include "configMigration.h"    // Legacy configuration migration
#include "updateRuntimeState.h"

configData actconf;             // Actual configuration, Global variable
                                // Overload with old EEPROM configuration by start. It is necessary for port and serial speed
                                // Don't change the position!

#include "Definitions.h"            // Global definitions
#include "func_myFunctions.h"       // Special functions
#include "func_webServerHandler.h"  // my lib for handle web server (deliver websites)
#include "GPS.h"                    // GPS parsing functions
#include "vedirect.h"               // VE.direct lib
#include "filesystem.h"             // Function for filesystem
#include "NMEATelegrams.h"          // Function library for NMEA telegrams
void noteManualLoraTxStarted();
bool completeManualLoraSend(bool acknowledged, uint8_t downlinkBytes);
bool failManualLoraSend(const char *reason);
#include "LoRa.h"                   // LoRa Lib

// Declarations
int value;                      // Value from first byte in EEPROM
int empty;                      // If EEPROM empty without configuration then set to 1 otherwise 0
configData defconf;             // Definition of default configuration data


AsyncWebServer httpServer(actconf.httpport);   // Port for HTTP server
//MDNSResponder mdns;                       // Activate DNS responder
WiFiServer server(actconf.dataport);        // Declare WiFi NMEA server port
#define MAX_CLIENTS 3 //maximal number of simultaneousy connected clients
WiFiClient clients[MAX_CLIENTS]; //Array of clients

Ticker Timer2;                  // Declare Timer for relay ontime
Ticker Timer3;                  // Declare Timer for NMEA sending
Ticker Timer4;                  // Declare Timer for Lora sending

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  actconf.standbySleepDuration       /* Time ESP32 will go to sleep */
struct MdsDeviceEventSnapshot {
  time_t standbyEpoch;
  time_t wakeupEpoch;
  char standbyCause[24];
  char wakeupCause[24];
};

const uint8_t MDS_DEVICE_EVENT_QUEUE_SIZE = 4;
const time_t MIN_RELIABLE_EVENT_TIME = 1777852800; // 2026-05-04 00:00:00 UTC
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR uint loraCount = 0;
RTC_DATA_ATTR time_t lastStandbyEventEpoch = 0;
RTC_DATA_ATTR time_t lastWakeupEventEpoch = 0;
RTC_DATA_ATTR char lastStandbyEventCause[24] = "";
RTC_DATA_ATTR char lastWakeupEventCause[24] = "";
RTC_DATA_ATTR bool pendingMdsDeviceEventStored = false;
RTC_DATA_ATTR time_t pendingMdsStandbyEventEpoch = 0;
RTC_DATA_ATTR time_t pendingMdsWakeupEventEpoch = 0;
RTC_DATA_ATTR char pendingMdsStandbyEventCause[24] = "";
RTC_DATA_ATTR char pendingMdsWakeupEventCause[24] = "";
RTC_DATA_ATTR time_t lastSentMdsStandbyEventEpoch = 0;
RTC_DATA_ATTR time_t lastSentMdsWakeupEventEpoch = 0;
RTC_DATA_ATTR time_t lastStandbyAutoUpdateCheckEpoch = 0;
RTC_DATA_ATTR MdsDeviceEventSnapshot pendingMdsDeviceEventQueue[MDS_DEVICE_EVENT_QUEUE_SIZE];
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueHead = 0;
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueTail = 0;
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueCount = 0;
RTC_DATA_ATTR bool keepAwakeAfterUpdateRestart = false;

const int STATE_DELAY = 1000;
bool reboot = false;
unsigned long standbySleepBlockedUntilMillis = 0;
bool localOtaInProgress = false;
unsigned long scheduledRestartMillis = 0;
bool sendedLoraAfterSleepOneTime = false;

bool toggleDisplayStatus = false;
long LoraSendDurationSeconds = 0;
volatile boolean runDownloadingFiles = false;
volatile boolean runDownloadingFilesStatus = false;
bool wifiServicesInitialized = false;
bool mdnsInitialized = false;
bool standbyWakeDisplayActive = false;
unsigned long standbyWakeDisplayHoldUntilMillis = 0;
bool standbyAutoUpdateAttemptedThisWake = false;

const unsigned long MDS_UPLOAD_INTERVAL_MS = 300000UL;
const unsigned long WAKE_MDS_RETRY_INTERVAL_MS = 30000UL;
const unsigned long STANDBY_WAKE_DISPLAY_HOLD_MS = 2500UL;
unsigned long lastMdsUploadMillis = 0;
bool pendingWakeMdsEvent = false;
int pendingWakeReasonCode = 0;
String pendingWakeReasonLabel = "unknown";
unsigned long nextWakeMdsRetryMillis = 0;
bool currentWakeMdsEventCaptured = false;
unsigned long currentWakeupBootMillis = 0;
bool currentWakeupBootMillisCaptured = false;

namespace {
constexpr uint8_t MANUAL_LORA_LOG_SIZE = 8;

struct ManualLoraLogEntry {
  uint32_t timestampMillis;
  char message[96];
};

volatile bool manualLoraSendRequested = false;
volatile bool manualLoraSendInProgress = false;
bool manualLoraRuntimeOwned = false;
uint32_t manualLoraRequestId = 0;
uint32_t manualLoraRequestedMillis = 0;
uint32_t manualLoraStartedMillis = 0;
uint32_t manualLoraCompletedMillis = 0;
uint32_t manualLoraResultHoldUntilMillis = 0;
bool manualLoraLastAcknowledged = false;
uint8_t manualLoraLastDownlinkBytes = 0;
char manualLoraState[20] = "idle";
char manualLoraMessage[128] = "Ready for a manual LoRa uplink.";
ManualLoraLogEntry manualLoraLog[MANUAL_LORA_LOG_SIZE] = {};
uint8_t manualLoraLogWriteIndex = 0;
uint8_t manualLoraLogCount = 0;
portMUX_TYPE manualLoraStatusMux = portMUX_INITIALIZER_UNLOCKED;

void addManualLoraLog(const String &message) {
  portENTER_CRITICAL(&manualLoraStatusMux);
  ManualLoraLogEntry &entry = manualLoraLog[manualLoraLogWriteIndex];
  entry.timestampMillis = millis();
  strlcpy(entry.message, message.c_str(), sizeof(entry.message));
  manualLoraLogWriteIndex = (manualLoraLogWriteIndex + 1) % MANUAL_LORA_LOG_SIZE;
  if (manualLoraLogCount < MANUAL_LORA_LOG_SIZE) {
    manualLoraLogCount++;
  }
  portEXIT_CRITICAL(&manualLoraStatusMux);
}

void setManualLoraStatus(const char *state, const String &message) {
  portENTER_CRITICAL(&manualLoraStatusMux);
  strlcpy(manualLoraState, state, sizeof(manualLoraState));
  strlcpy(manualLoraMessage, message.c_str(), sizeof(manualLoraMessage));
  portEXIT_CRITICAL(&manualLoraStatusMux);
  addManualLoraLog(message);
  DebugPrintln(3, "Manual LoRa: " + message);
}

bool hasConfiguredLoraSession() {
  if (actconf.devaddr == 0) {
    return false;
  }

  bool hasNetworkKey = false;
  bool hasApplicationKey = false;
  for (size_t i = 0; i < sizeof(actconf.nskey); i++) {
    hasNetworkKey = hasNetworkKey || actconf.nskey[i] != 0;
    hasApplicationKey = hasApplicationKey || actconf.appkey[i] != 0;
  }
  return hasNetworkKey && hasApplicationKey;
}
}

long timezone = 1;
byte daysavetime = 1;

bool hasValidWakeSleepTime();
bool processPendingRemoteOta(const char *contextLabel, bool restartImmediately);
bool isManualLoraSendBusy();
bool isManualLoraKeepAwake();
bool processManualLoraSend();

bool hasActiveStandbyKeepAwakeWork() {
  const RemoteOtaSnapshot ota = getRemoteOtaSnapshot();
  return localOtaInProgress ||
         ota.pending ||
         ota.inProgress ||
         runDownloadingFiles ||
         runDownloadingFilesStatus ||
         scheduledRestartMillis > 0 ||
         isManualLoraKeepAwake() ||
         reboot;
}

File root;

void copyEventCause(char *target, size_t targetSize, const char *eventCause) {
  if (target == nullptr || targetSize == 0) {
    return;
  }

  if (eventCause != nullptr) {
    strncpy(target, eventCause, targetSize - 1);
    target[targetSize - 1] = '\0';
  } else {
    target[0] = '\0';
  }
}

void copyMdsDeviceEventToActive(const MdsDeviceEventSnapshot &event) {
  pendingMdsStandbyEventEpoch = event.standbyEpoch;
  pendingMdsWakeupEventEpoch = event.wakeupEpoch;
  copyEventCause(pendingMdsStandbyEventCause, sizeof(pendingMdsStandbyEventCause), event.standbyCause);
  copyEventCause(pendingMdsWakeupEventCause, sizeof(pendingMdsWakeupEventCause), event.wakeupCause);
  pendingMdsDeviceEventStored = true;
}

bool mdsDeviceEventMatches(const MdsDeviceEventSnapshot &event, time_t standbyEpoch, time_t wakeupEpoch) {
  return event.standbyEpoch == standbyEpoch && event.wakeupEpoch == wakeupEpoch;
}

bool isMdsDeviceEventAlreadyQueued(time_t standbyEpoch, time_t wakeupEpoch) {
  if (pendingMdsDeviceEventStored && pendingMdsStandbyEventEpoch == standbyEpoch && pendingMdsWakeupEventEpoch == wakeupEpoch) {
    return true;
  }

  for (uint8_t i = 0; i < pendingMdsDeviceEventQueueCount; i++) {
    const uint8_t index = (pendingMdsDeviceEventQueueHead + i) % MDS_DEVICE_EVENT_QUEUE_SIZE;
    if (mdsDeviceEventMatches(pendingMdsDeviceEventQueue[index], standbyEpoch, wakeupEpoch)) {
      return true;
    }
  }

  return false;
}

void enqueueMdsDeviceEvent(time_t standbyEpoch, time_t wakeupEpoch, const char *standbyCause, const char *wakeupCause) {
  if (pendingMdsDeviceEventQueueCount >= MDS_DEVICE_EVENT_QUEUE_SIZE) {
    DebugPrintln(2, "MDS device event queue full, dropping oldest wakeup event");
    pendingMdsDeviceEventQueueHead = (pendingMdsDeviceEventQueueHead + 1) % MDS_DEVICE_EVENT_QUEUE_SIZE;
    pendingMdsDeviceEventQueueCount--;
  }

  MdsDeviceEventSnapshot &event = pendingMdsDeviceEventQueue[pendingMdsDeviceEventQueueTail];
  event.standbyEpoch = standbyEpoch;
  event.wakeupEpoch = wakeupEpoch;
  copyEventCause(event.standbyCause, sizeof(event.standbyCause), standbyCause);
  copyEventCause(event.wakeupCause, sizeof(event.wakeupCause), wakeupCause);

  pendingMdsDeviceEventQueueTail = (pendingMdsDeviceEventQueueTail + 1) % MDS_DEVICE_EVENT_QUEUE_SIZE;
  pendingMdsDeviceEventQueueCount++;
}

bool loadNextQueuedMdsDeviceEvent() {
  if (pendingMdsDeviceEventStored) {
    return true;
  }

  if (pendingMdsDeviceEventQueueCount == 0) {
    return false;
  }

  copyMdsDeviceEventToActive(pendingMdsDeviceEventQueue[pendingMdsDeviceEventQueueHead]);
  pendingMdsDeviceEventQueueHead = (pendingMdsDeviceEventQueueHead + 1) % MDS_DEVICE_EVENT_QUEUE_SIZE;
  pendingMdsDeviceEventQueueCount--;
  return true;
}

void IRAM_ATTR onTimer(){
  DebugPrintln(3, "onTimer reached");
  sendLoraQueue = true;
}

void registerStandbyEvent(const char *eventCause) {
  lastStandbyEventEpoch = time(nullptr);
  copyEventCause(lastStandbyEventCause, sizeof(lastStandbyEventCause), eventCause);
}

void registerWakeupEvent(const char *eventCause) {
  lastWakeupEventEpoch = time(nullptr);
  copyEventCause(lastWakeupEventCause, sizeof(lastWakeupEventCause), eventCause);
}

void markWakeupBootMoment(const char *eventCause) {
  currentWakeupBootMillis = millis();
  currentWakeupBootMillisCaptured = true;
  lastWakeupEventEpoch = 0;
  copyEventCause(lastWakeupEventCause, sizeof(lastWakeupEventCause), eventCause);
}

bool ensureWakeupEventTimestamp(const char *eventCause) {
  if (lastWakeupEventEpoch > 0) {
    return true;
  }

  if (!currentWakeupBootMillisCaptured || !hasValidWakeSleepTime()) {
    return false;
  }

  const unsigned long elapsedSinceWakeMs = millis() - currentWakeupBootMillis;
  lastWakeupEventEpoch = time(nullptr) - static_cast<time_t>((elapsedSinceWakeMs + 500UL) / 1000UL);
  if ((eventCause != nullptr) && eventCause[0] != '\0') {
    copyEventCause(lastWakeupEventCause, sizeof(lastWakeupEventCause), eventCause);
  }
  DebugPrintln(3, "Wakeup timestamp reconstructed after time sync: " + String(static_cast<unsigned long>(lastWakeupEventEpoch)));
  return lastWakeupEventEpoch > 0;
}

bool isDeepSleepWakeup(esp_sleep_wakeup_cause_t wakeupReason) {
  switch (wakeupReason) {
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
    case ESP_SLEEP_WAKEUP_TIMER:
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    case ESP_SLEEP_WAKEUP_ULP:
      return true;
    default:
      return false;
  }
}

bool capturePendingMdsDeviceEvent(const char *eventCause) {
  if (!ensureWakeupEventTimestamp(eventCause)) {
    DebugPrintln(2, "Skipping MDS wakeup event because no wakeup timestamp is available");
    return false;
  }

  if (lastStandbyEventEpoch <= 0) {
    DebugPrintln(2, "Skipping MDS wakeup event because no standby timestamp is available");
    return false;
  }

  if ((eventCause != nullptr) && lastWakeupEventCause[0] == '\0') {
    copyEventCause(lastWakeupEventCause, sizeof(lastWakeupEventCause), eventCause);
  }

  if (lastStandbyEventEpoch > lastWakeupEventEpoch) {
    DebugPrintln(2, "Skipping MDS wakeup event because standby time is later than wakeup time");
    return false;
  }

  if (lastStandbyEventEpoch == lastSentMdsStandbyEventEpoch && lastWakeupEventEpoch == lastSentMdsWakeupEventEpoch) {
    DebugPrintln(2, "Skipping duplicate MDS wakeup event");
    return false;
  }

  if (isMdsDeviceEventAlreadyQueued(lastStandbyEventEpoch, lastWakeupEventEpoch)) {
    DebugPrintln(2, "Skipping already queued MDS wakeup event");
    return false;
  }

  enqueueMdsDeviceEvent(lastStandbyEventEpoch, lastWakeupEventEpoch, lastStandbyEventCause, lastWakeupEventCause);
  currentWakeMdsEventCaptured = true;
  return true;
}

bool waitForValidSystemTime(uint32_t timeoutMs) {
  const uint32_t start = millis();
  time_t now = time(nullptr);
  while (now < MIN_RELIABLE_EVENT_TIME && (millis() - start) < timeoutMs) {
    delay(250);
    now = time(nullptr);
  }
  return now >= MIN_RELIABLE_EVENT_TIME;
}

bool hasValidWakeSleepTime() {
  return time(nullptr) >= MIN_RELIABLE_EVENT_TIME;
}

bool canUpdateStandbyWakeDisplay(bool force = false) {
  if (!standbyWakeDisplayActive) {
    return false;
  }

  if (force) {
    return true;
  }

  return millis() >= standbyWakeDisplayHoldUntilMillis;
}

void showStandbyWakeDisplay(const String &line1,
                            const String &line2 = "",
                            const String &line3 = "",
                            const String &line4 = "",
                            bool force = false) {
  if (!canUpdateStandbyWakeDisplay(force)) {
    return;
  }

  writeDisplayStatusScreen("Standby wake", line1, line2, line3, line4);
}

void stopMdnsService() {
  if (!mdnsInitialized) {
    return;
  }

  MDNS.end();
  mdnsInitialized = false;
  DebugPrintln(2, "mDNS service stopped");
}

void ensureMdnsService() {
  if (actconf.mDNS != 1) {
    stopMdnsService();
    return;
  }

  if (WiFi.status() != WL_CONNECTED || WiFi.localIP().toString() == "0.0.0.0") {
    stopMdnsService();
    return;
  }

  if (mdnsInitialized) {
    return;
  }

  MDNS.end();
  if (!MDNS.begin(hname.c_str())) {
    DebugPrintln(1, "Failed to start mDNS responder");
    return;
  }

  MDNS.setInstanceName(hname);
  MDNS.addService("http", "tcp", actconf.httpport);
  MDNS.addServiceTxt("http", "tcp", "path", "/");
  MDNS.addService("nmea-0183", "tcp", actconf.dataport);
  MDNS.enableWorkstation(ESP_IF_WIFI_STA);
  mdnsInitialized = true;

  DebugPrintln(3, "mDNS service: active");
  DebugPrintln(3, "mDNS name: " + hname + ".local");
  DebugPrintln(3, "mDNS URL: http://" + hname + ".local/");
}

void initWiFiServicesOnce() {
  if (wifiServicesInitialized) {
    return;
  }

  WebServerHandler();
  // Register WebSerial on the same server instance that is started below.
  WebSerial.begin(&httpServer);

  httpServer.begin();
  server.begin();
  Timer3.attach_ms(SendPeriod, sendNMEA);

  wifiServicesInitialized = true;
}

void maybeSendDataViaWifi() {
  if (wifiServicesInitialized) {
    ensureMdnsService();
  }

  if (String(actconf.SendDataViaWifi) != "Yes") {
    return;
  }

  const unsigned long now = millis();

  if (now >= nextWakeMdsRetryMillis && loadNextQueuedMdsDeviceEvent()) {
    showStandbyWakeDisplay("Send wake log", pendingWakeReasonLabel, "Uploading MDS");
    if (sendMdsDeviceEvent(actconf, pendingWakeReasonLabel.c_str())) {
      lastSentMdsStandbyEventEpoch = pendingMdsStandbyEventEpoch;
      lastSentMdsWakeupEventEpoch = pendingMdsWakeupEventEpoch;
      pendingMdsDeviceEventStored = false;
      currentWakeMdsEventCaptured = false;
      nextWakeMdsRetryMillis = 0;
      showStandbyWakeDisplay("Wake log sent", pendingWakeReasonLabel, "MDS upload ok");
    } else {
      nextWakeMdsRetryMillis = now + WAKE_MDS_RETRY_INTERVAL_MS;
      showStandbyWakeDisplay("Wake log failed", "Retry pending", "Check WiFi");
    }
  }

  if ((lastMdsUploadMillis == 0) || (now - lastMdsUploadMillis >= MDS_UPLOAD_INTERVAL_MS)) {
    showStandbyWakeDisplay("Send sensors", "WiFi -> MDS");
    lastMdsUploadMillis = now;
    if (sendToMDS(actconf)) {
      showStandbyWakeDisplay("Sensors sent", "Back to sleep");
    } else {
      nextWakeMdsRetryMillis = now + WAKE_MDS_RETRY_INTERVAL_MS;
      showStandbyWakeDisplay("Sensor upload", "Failed", "Retry later");
    }
  }
}

bool isWifiFirstTransmitPriority() {
  return String(actconf.transmitPriority) == "WifiFirst";
}

bool isLoraEnabledInStandby() {
  return String(actconf.loraOperationMode) == "Standby" || String(actconf.loraOperationMode) == "Always";
}

bool isLoraEnabledInPowerOn() {
  return String(actconf.loraOperationMode) == "Always" || String(actconf.loraOperationMode) == "PowerOn";
}

bool isMdsUploadEnabled() {
  return String(actconf.SendDataViaWifi) == "Yes";
}

bool isStandbyAutoUpdateEnabled() {
  return String(actconf.standbyAutoUpdate) == "Yes";
}

bool areInstalledWebFilesOutdated() {
  return !areWebFilesCurrent(actconf.fversion, getFirmwareReleaseChannel().c_str());
}

bool isStandbyAutoFirmwareCheckDue() {
  if (!isStandbyAutoUpdateEnabled() || !hasValidWakeSleepTime()) {
    return false;
  }

  const time_t now = time(nullptr);
  const int intervalHours = actconf.standbyAutoUpdateIntervalHours < 1 ? 24 : actconf.standbyAutoUpdateIntervalHours;
  const time_t intervalSeconds = static_cast<time_t>(intervalHours) * 3600;
  return lastStandbyAutoUpdateCheckEpoch == 0 || now - lastStandbyAutoUpdateCheckEpoch >= intervalSeconds;
}

bool shouldEnableWifiForStandbyAutoUpdate() {
  return isStandbyAutoUpdateEnabled() && (isStandbyAutoFirmwareCheckDue() || areInstalledWebFilesOutdated());
}

bool processStandbyAutoUpdate() {
  if (!isStandbyAutoUpdateEnabled() || standbyAutoUpdateAttemptedThisWake) {
    return false;
  }

  const bool webFilesOutdated = areInstalledWebFilesOutdated();
  const bool firmwareCheckDue = isStandbyAutoFirmwareCheckDue();
  if (!webFilesOutdated && !firmwareCheckDue) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DebugPrintln(2, "Skipping standby auto update because WiFi is not connected");
    return false;
  }

  if (!hasValidWakeSleepTime()) {
    DebugPrintln(2, "Skipping standby auto update because system time is not synchronized");
    return false;
  }

  standbyAutoUpdateAttemptedThisWake = true;
  standbySleepBlockedUntilMillis = millis() + 180000UL;
  writeDisplayStatusScreen("Auto update", "Checking MDS", getFirmwareReleaseLabel(), String(actconf.fversion));
  DebugPrintln(3, "Standby auto update check started");

  if (firmwareCheckDue) {
    String remoteUrl;
    String remoteVersion;
    String remoteSha256;
    String webFilesPath;
    String resolveError;
    const String channel = getFirmwareReleaseChannel();
    lastStandbyAutoUpdateCheckEpoch = time(nullptr);

    if (resolveOtaFirmwareForChannel(channel, remoteUrl, remoteVersion, &resolveError, &remoteSha256, &webFilesPath)) {
      const int versionComparison = compareOtaFirmwareVersions(remoteVersion, String(actconf.fversion));
      if (versionComparison > 0) {
        remoteSha256 = normalizeSha256String(remoteSha256);
        if (remoteSha256.length() == 64) {
          writeDisplayStatusScreen("Auto update", "Firmware found", remoteVersion, "Installing");
          RemoteOtaRequest request;
          request.url = remoteUrl;
          request.version = remoteVersion;
          request.sha256 = remoteSha256;
          request.channel = channel;
          if (!acquireMaintenanceOperation(MaintenanceOperation::RemoteOta) || !queueRemoteOtaRequest(request)) {
            releaseMaintenanceOperation(MaintenanceOperation::RemoteOta);
            DebugPrintln(1, "Automatic firmware update could not be queued because another update is active");
            return false;
          }
          DebugPrintln(3, "Automatic firmware update queued: " + remoteVersion);
          return processPendingRemoteOta("Auto", true);
        }
        DebugPrintln(1, "Skipping automatic firmware update because manifest SHA256 is missing or invalid");
      } else {
        DebugPrintln(3, "No newer firmware available for automatic standby update");
      }
    } else {
      DebugPrintln(1, "Automatic firmware update check failed: " + resolveError);
    }
  }

  if (areInstalledWebFilesOutdated()) {
    if (!acquireMaintenanceOperation(MaintenanceOperation::WebFiles)) {
      DebugPrintln(2, "Skipping automatic web files update because maintenance is busy");
      return false;
    }
    writeDisplayStatusScreen("Auto update", "Web files", "Downloading", String(actconf.fversion));
    DebugPrintln(3, "Automatic web files update started");
    const bool success = DownloadFilesFromWeb();
    releaseMaintenanceOperation(MaintenanceOperation::WebFiles);
    writeDisplayStatusScreen("Auto update",
                             success ? "Web files OK" : "Web files failed",
                             String(actconf.fversion),
                             success ? "Done" : "Retry later");
    finishDisplayProgressMode(success ? 5000UL : 10000UL);
    return success;
  }

  writeDisplayStatusScreen("Auto update", "Already current", getFirmwareReleaseLabel(), String(actconf.fversion));
  return false;
}

bool processPendingRemoteOta(const char *contextLabel, bool restartImmediately) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  RemoteOtaRequest request;
  if (!beginRemoteOtaRequest(request)) {
    return false;
  }

  // Prefer the channel manifest because it provides an explicit version and
  // SHA256 before flash writing starts. Keep the authenticated MDS endpoint as
  // a compatibility fallback when metadata cannot be resolved.
  if (request.useMdsEndpoint && request.channel.length() > 0) {
    String manifestUrl;
    String manifestVersion;
    String manifestSha256;
    String manifestError;
    if (resolveOtaFirmwareForChannel(request.channel,
                                     manifestUrl,
                                     manifestVersion,
                                     &manifestError,
                                     &manifestSha256,
                                     nullptr)) {
      const int versionComparison = compareOtaFirmwareVersions(manifestVersion, String(actconf.fversion));
      if (versionComparison > 0 || (versionComparison == 0 && request.forceReinstall)) {
        request.url = manifestUrl;
        request.version = manifestVersion;
        request.sha256 = manifestSha256;
        request.useMdsEndpoint = false;
        setRemoteOtaVersion(manifestVersion);
        DebugPrintln(3, String(contextLabel) + " OTA resolved from manifest: " + manifestVersion);
      } else {
        const String statusMessage = versionComparison == 0
          ? "Installed firmware already matches the selected channel."
          : "Selected channel contains older firmware; update skipped.";
        OtaProgressSnapshot state;
        state.success = true;
        state.phase = "no-update";
        state.message = statusMessage;
        setOtaProgressState(state);
        finishRemoteOtaRequest(false);
        releaseMaintenanceOperation(MaintenanceOperation::RemoteOta);
        DebugPrintln(3, String(contextLabel) + " OTA: " + statusMessage);
        return true;
      }
    } else {
      DebugPrintln(2, String(contextLabel) + " OTA manifest resolution failed; using authenticated endpoint: " + manifestError);
    }
  }

  String remoteOtaError;
  const bool remoteOtaSuccess = performRemoteOtaUpdate(request.url, false, remoteOtaError, request.sha256, request.useMdsEndpoint);
  flushPendingRemoteOtaStatus();
  const bool rebootRequired = getRemoteOtaSnapshot().rebootRequired;
  finishRemoteOtaRequest(rebootRequired);
  releaseMaintenanceOperation(MaintenanceOperation::RemoteOta);

  if (remoteOtaSuccess && rebootRequired) {
    DebugPrintln(3, String(contextLabel) + " OTA update completed, reboot scheduled");
    if (restartImmediately) {
      ESP.restart();
    }
    scheduledRestartMillis = millis() + 6000UL;
    reboot = false;
    return true;
  }

  if (remoteOtaSuccess) {
    DebugPrintln(3, String(contextLabel) + " OTA update completed without reboot requirement");
    return true;
  }

  DebugPrintln(1, String(contextLabel) + " OTA update failed: " + remoteOtaError);
  return false;
}

void disableWiFiForSleep() {
  Timer3.detach();
  server.stop();
  stopMdnsService();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
}

void configureAnalogInputs() {
  analogReadResolution(12);
  analogSetPinAttenuation(uint8_t(ANALOG_IN), ADC_11db);
  analogSetPinAttenuation(uint8_t(TANK1_IN), ADC_11db);
  analogSetPinAttenuation(uint8_t(TANK2_IN), ADC_11db);

  pinMode(ANALOG_IN, INPUT);
  pinMode(TANK1_IN, INPUT);
  pinMode(TANK2_IN, INPUT);

  // Discard the first sample after reconfiguring the ADC path.
  analogRead(ANALOG_IN);
  analogRead(TANK1_IN);
  analogRead(TANK2_IN);
}

void prepareForStandbySleep() {
  DebugPrintln(3, "Prepare standby sleep");
  writeDisplayStatusScreen("Standby", "Going to sleep", pendingWakeReasonLabel, "No pending tasks");
  delay(2200);

  if (pendingWakeMdsEvent && !currentWakeMdsEventCaptured && hasValidWakeSleepTime()) {
    capturePendingMdsDeviceEvent(pendingWakeReasonLabel.c_str());
    pendingWakeMdsEvent = false;
  }

  if (String(actconf.SendDataViaWifi) == "Yes" && hasValidWakeSleepTime()) {
    registerStandbyEvent("Sleep standby");
    if (pendingWakeMdsEvent && !currentWakeMdsEventCaptured) {
      capturePendingMdsDeviceEvent(pendingWakeReasonLabel.c_str());
      pendingWakeMdsEvent = false;
    }
  } else if (String(actconf.SendDataViaWifi) == "Yes") {
    DebugPrintln(2, "Skipping standby timestamp because system time is not synchronized");
  }

  // Re-arm wake sources right before sleep so runtime config changes
  // like standbySleepDuration apply without needing a reboot.
  const uint64_t sleepDurationUs = uint64_t(max(1, actconf.standbySleepDuration)) * 60ULL * uint64_t(uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup(sleepDurationUs);
  rtc_gpio_pullup_en(GPIO_NUM_39);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0);

  if (String(actconf.WifiStandbyMode) == "Yes") {
    disableWiFiForSleep();
  }

  if (String(actconf.relay) == "2" || actconf.relay == 2) {
    UBLOX_GPS_Shutdown();
    delay(100);
  }

  digitalWrite(relayPin, LOW);
  u8x8.clearDisplay();
  u8x8.setPowerSave(1);
  standbyWakeDisplayActive = false;
  standbyWakeDisplayHoldUntilMillis = 0;
}

struct WifiCandidate {
  int slot;
  int priority;
  const char *ssid;
  const char *password;
};

int getWifiPriorityForSlot(int slot) {
  if (slot == 1) return actconf.corder1;
  if (slot == 2) return actconf.corder2;
  if (slot == 3) return actconf.corder3;
  return 0;
}

void setWifiPriorityForSlot(int slot, int priority) {
  if (slot == 1) actconf.corder1 = priority;
  if (slot == 2) actconf.corder2 = priority;
  if (slot == 3) actconf.corder3 = priority;
}

void sortWifiCandidates(WifiCandidate candidates[], size_t count) {
  for (size_t outer = 0; outer < count; outer++) {
    for (size_t inner = outer + 1; inner < count; inner++) {
      const int outerPriority = candidates[outer].priority <= 0 ? 99 : candidates[outer].priority;
      const int innerPriority = candidates[inner].priority <= 0 ? 99 : candidates[inner].priority;
      if (innerPriority < outerPriority ||
          (innerPriority == outerPriority && candidates[inner].slot < candidates[outer].slot)) {
        WifiCandidate temp = candidates[outer];
        candidates[outer] = candidates[inner];
        candidates[inner] = temp;
      }
    }
  }
}

void promoteSuccessfulWifiSlot(int successfulSlot) {
  if (successfulSlot < 1 || successfulSlot > 3 || getWifiPriorityForSlot(successfulSlot) == 0) {
    return;
  }

  WifiCandidate candidates[3] = {
    {1, actconf.corder1, actconf.cssid1, actconf.cpassword1},
    {2, actconf.corder2, actconf.cssid2, actconf.cpassword2},
    {3, actconf.corder3, actconf.cssid3, actconf.cpassword3}
  };
  sortWifiCandidates(candidates, 3);

  int newPriorities[4] = {0, 0, 0, 0};
  int nextPriority = 1;
  newPriorities[successfulSlot] = nextPriority++;
  for (size_t index = 0; index < 3; index++) {
    const int slot = candidates[index].slot;
    if (slot == successfulSlot || candidates[index].priority <= 0) {
      continue;
    }
    newPriorities[slot] = nextPriority++;
  }

  bool changed = false;
  for (int slot = 1; slot <= 3; slot++) {
    if (getWifiPriorityForSlot(slot) != newPriorities[slot]) {
      changed = true;
      setWifiPriorityForSlot(slot, newPriorities[slot]);
    }
  }

  if (changed) {
    saveEEPROMConfig(actconf);
    DebugPrint(3, "WiFi priority updated. Last successful slot: ");
    DebugPrintln(3, successfulSlot);
  }
}

void enableWiFi(bool startNetworkServices = true){
  if (wifiServicesInitialized && WiFi.status() == WL_CONNECTED) {
    if (startNetworkServices) {
      ensureMdnsService();
      DebugPrintln(3, "WiFi already active, keeping existing network services running.");
    } else {
      DebugPrintln(3, "WiFi already active for background upload.");
    }
    return;
  }

  //adc_power_on();
  hname = String(actconf.hostname) + "-" + String(actconf.deviceID);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);  // Reconnect the network without turning WiFi off.
  WiFi.hostname(hname);   // Provide the hostname
  //WiFi.setHostname (hname.c_str()); /*ESP32 hostname set*/

  //*****************************************************************************************
      DebugPrint(3, "Host name: ");
      DebugPrintln(3, hname);

      DebugPrintln(3, "Scanning WiFi networks...");
      int networkCount = WiFi.scanNetworks(false, true);
      if (networkCount <= 0) {
        DebugPrintln(2, "No WiFi networks found during scan");
      } else {
        DebugPrint(3, "WiFi networks found: ");
        DebugPrintln(3, networkCount);
        for (int networkIndex = 0; networkIndex < networkCount; networkIndex++) {
          DebugPrint(3, "  SSID: ");
          DebugPrint(3, WiFi.SSID(networkIndex));
          DebugPrint(3, ", RSSI: ");
          DebugPrint(3, WiFi.RSSI(networkIndex));
          DebugPrint(3, ", channel: ");
          DebugPrint(3, WiFi.channel(networkIndex));
          DebugPrint(3, ", auth: ");
          DebugPrintln(3, WiFi.encryptionType(networkIndex));
        }
      }

      WifiCandidate wifiCandidates[3] = {
        {1, actconf.corder1, actconf.cssid1, actconf.cpassword1},
        {2, actconf.corder2, actconf.cssid2, actconf.cpassword2},
        {3, actconf.corder3, actconf.cssid3, actconf.cpassword3}
      };
      sortWifiCandidates(wifiCandidates, 3);
      
      for (size_t index = 0; index < 3; index++) {
        WifiCandidate candidate = wifiCandidates[index];
        if (candidate.priority <= 0) {
          DebugPrint(3, "Skipping disabled WiFi #");
          DebugPrintln(3, candidate.slot);
          continue;
        }

        char cssid[31];
        char cpassword[31];
        strncpy(cssid, candidate.ssid, sizeof(cssid) - 1);
        strncpy(cpassword, candidate.password, sizeof(cpassword) - 1);
        cssid[sizeof(cssid) - 1] = '\0';
        cpassword[sizeof(cpassword) - 1] = '\0';

        if (cssid[0] == '\0') {
          DebugPrint(3, "Skipping empty WiFi #");
          DebugPrintln(3, candidate.slot);
          continue;
        }

        bool configuredSsidVisible = false;
        for (int networkIndex = 0; networkIndex < networkCount; networkIndex++) {
          if (WiFi.SSID(networkIndex) == String(cssid)) {
            configuredSsidVisible = true;
            break;
          }
        }
        DebugPrint(3, "Configured SSID visible in scan: ");
        DebugPrintln(3, configuredSsidVisible ? "yes" : "no");

        // Connect to WiFi network
        DebugPrint(3, "Connecting WiFi #");
        DebugPrint(3, candidate.slot);
        DebugPrint(3, " priority ");
        DebugPrint(3, candidate.priority);
        DebugPrint(3, " client to ");
        DebugPrintln(3, cssid);

        u8x8.clearLine(5);
        u8x8.drawString(0,5, cssid);
        u8x8.refreshDisplay();    // Only required for SSD1606/7

        // Load connection timeout from configuration (maxccount = (timeout[s] * 1000) / 200[ms])
        const int effectiveTimeoutSeconds = constrain(actconf.timeout, 3, 15);
        maxccounter = ((effectiveTimeoutSeconds * 1000) / 200);
        if (networkCount > 0 && !configuredSsidVisible) {
          maxccounter = min(maxccounter, 10);
        }

        // Wait until is connected otherwise abort connection after x connection trys
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(false, false);
        delay(100);
        WiFi.begin(cssid, cpassword);
        ccounter = 0;
        boolean toggleWifiConnectionStatus = false;
        while ((WiFi.status() != WL_CONNECTED) && (ccounter <= maxccounter)) {
          delay(200);
          DebugPrint(3, ".");
          if (toggleWifiConnectionStatus) {
            u8x8.drawString(0,6,".");
            toggleWifiConnectionStatus = false;
          } else {
            u8x8.drawString(0,6," ");
            toggleWifiConnectionStatus = true;
          }
          ccounter ++;
        }
        u8x8.drawString(0,6," ");
        DebugPrintln(3, "");
        if (WiFi.status() == WL_CONNECTED){
          DebugPrint(3, "WiFi client connected with IP: ");
          DebugPrintln(3, WiFi.localIP());
          DebugPrintln(3, "");
          u8x8.clearLine(4);
          u8x8.clearLine(5);
          u8x8.clearLine(6);
          if (startNetworkServices) {
            u8x8.drawString(0,5,"IP address:");
            u8x8.drawString(0,6, WiFi.localIP().toString().c_str());
          } else {
            u8x8.drawString(0,4,"WiFi/MDS only");
            u8x8.drawString(0,5,"Web server off");
            u8x8.drawString(0,6,"Sleep shortly");
          }
          u8x8.refreshDisplay();    // Only required for SSD1606/7
          promoteSuccessfulWifiSlot(candidate.slot);
          delay(startNetworkServices ? 100 : 1800);
          DebugPrintln(3, "Exit loop");
          break;
        }
        else{
          DebugPrint(3, "WiFi status after abort: ");
          DebugPrintln(3, WiFi.status());
          WiFi.disconnect(false, false);        // Abort this STA attempt, keep AP+WiFi alive for the next SSID.
          DebugPrintln(3, "Connection aborted");
          DebugPrintln(3, "");
          //u8x8.drawString(0,3,"Conection aborted");
          //u8x8.refreshDisplay();    // Only required for SSD1606/7
        }
      }

      DebugPrintln(3, "Contacting Time Server");
	    configTime(3600*timezone, daysavetime*3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
	    struct tm tmstruct;
      if (waitForValidSystemTime(5000) && getLocalTime(&tmstruct, 1000)) {
        DebugPrintln(3, "\nNow is : " + String((tmstruct.tm_year)+1900) + "-" + String((tmstruct.tm_mon)+1) + "-" + String(tmstruct.tm_mday) + " " + String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));
      } else {
        DebugPrintln(1, "Time sync failed. HTTPS requests may be postponed until NTP is available.");
      }

      if (!startNetworkServices) {
        stopMdnsService();
        writeDisplayStatusScreen("Standby WiFi",
                                 WiFi.status() == WL_CONNECTED ? "MDS upload only" : "No WiFi",
                                 "Web server off",
                                 "Going sleep soon");
        DebugPrintln(3, "WiFi background mode: AP, mDNS and web server are not started.");
        return;
      }

      const bool wifiClientConnected = WiFi.status() == WL_CONNECTED;
      const uint8_t apChannel = wifiClientConnected ? WiFi.channel() : actconf.apchannel;
      WiFi.mode(WIFI_MODE_APSTA);
      WiFi.softAP(actconf.sssid, actconf.spassword, apChannel, false, actconf.maxconnections);

      // Starting access point for update server
      DebugPrint(3, "Access point started with SSID: ");
      DebugPrintln(3, actconf.sssid);
      DebugPrint(3, "Access point channel: ");
      DebugPrintln(3, apChannel);
      DebugPrint(3, "Max AP connections: ");
      DebugPrintln(3, actconf.maxconnections);

      ensureMdnsService();

      initWiFiServicesOnce();

      DebugPrint(3, "HTTP Update Server started at port: ");
      DebugPrintln(3, actconf.httpport);
      DebugPrint(3, "Use this URL: ");
      DebugPrint(3, "http://");
      DebugPrint(3, WiFi.softAPIP());
      DebugPrintln(3, "/update");
      DebugPrintln(3, "");
      
      // Start the NMEA TCP server
      DebugPrint(3, "NMEA-Server started at port: ");
      DebugPrintln(3, actconf.dataport);
      // Print the IP address
      DebugPrint(3, "Use this URL : ");
      DebugPrint(3, "http://");
      if (WiFi.status() == WL_CONNECTED){
        DebugPrintln(3, WiFi.localIP());
      }
      else{
        DebugPrintln(3, WiFi.softAPIP());
      };
      DebugPrintln(3, "");

      // TCP-Server for NMEA0183
      //client = server.available();// Check if a client is connected
    //*****************************************************************************************
}

void VEdirectSend()
{
  boolean debugPrintValues = false;
// Send VE.direct data all 1s
  if(millis() > starttime0 + 1000){
    static int count;
    starttime0 = millis();          // Read actual time
    if (String(actconf.envSensor) == "VEdirect-Send" && !isVEdirectBinaryBusy()) {
      if (debugPrintValues) {
        DebugPrintln(3, "VE.direct Output");
      }
      sendVEdirect();               // Send VE.direct text data
      // ":78DED000B05C4\n"
      int voltageOut = voltage * 100;
      sendBinaryValue(":78DED00", voltageOut); // Send binary data
      if(count == 0){
        queueVEdirectBinary();      // Send the long setup sequence without blocking the main loop.
      }
    }
    count ++;
    count = count % 10;
  }

}

void VEdirectRead()
{
  boolean debugPrintValues = false;
// Read VE.direct values (BMV-712 tested)
  if (String(actconf.envSensor) == "VEdirect-Read") {
    static String receivedLine;

    while (Serial1.available()) {
      const char rc = char(Serial1.read());

      if (rc == '\r') {
        continue;
      }

      if (rc == '\n') {
        receivedLine.trim();

        const int separator = receivedLine.indexOf('\t');
        if (separator > 0) {
          const String field = receivedLine.substring(0, separator);
          const String value = receivedLine.substring(separator + 1);
          const long parsedValue = value.toInt();

          if (field == "V" && parsedValue > 712) {
            vedirectVoltage = parsedValue / 1000.0;
            if (debugPrintValues) {
              DebugPrintln(3, "VE.direct V: " + String(vedirectVoltage, 3));
            }
          }
          else if (field == "I") {
            vedirectCurrent = parsedValue / 1000.0;
            if (debugPrintValues) {
              DebugPrintln(3, "VE.direct I: " + String(vedirectCurrent));
            }
          }
          else if (field == "P") {
            vedirectPower = parsedValue;
            if (debugPrintValues) {
              DebugPrintln(3, "VE.direct P: " + String(vedirectPower));
            }
          }
          else if (field == "SOC") {
            vedirectSOC = parsedValue;
            if (debugPrintValues) {
              DebugPrintln(3, "VE.direct SOC: " + String(vedirectSOC));
            }
          }
          else if (field == "T") {
            vedirectTemp = parsedValue / 10.0;
            if (debugPrintValues) {
              DebugPrintln(3, "VE.direct T: " + String(vedirectTemp));
            }
          }
        }

        receivedLine = "";
      }
      else {
        receivedLine += rc;
        if (receivedLine.length() > 80) {
          receivedLine = "";
        }
      }
    }
  }
}

// copied from task.h
void lora_init() {
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();
  LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);  // For better receiving results

  // Set static session parameters. Instead of dynamically establishing a session
  // by joining the network, precomputed session parameters are be provided.
  #ifdef PROGMEM
  // On AVR, these values are stored in flash and only copied to RAM
  // once. Copy them to a temporary buffer here, LMIC_setSession will
  // copy them into a buffer of its own again.
  uint8_t appskey[sizeof(actconf.appkey)];
  uint8_t nwkskey[sizeof(actconf.nskey)];
  memcpy_P(appskey, actconf.appkey, sizeof(actconf.appkey));
  memcpy_P(nwkskey, actconf.nskey, sizeof(actconf.nskey));
  LMIC_setSession (0x1, actconf.devaddr, nwkskey, appskey);
  #else
  // If not running an AVR with PROGMEM, just use the arrays directly
  LMIC_setSession (0x1, actconf.devaddr, actconf.nskey, actconf.appkey);
  #endif

  #if defined(CFG_eu868)
  // Set up the channels used by the Things Network, which corresponds
  // to the defaults of most gateways. Without this, only three base
  // channels from the LoRaWAN specification are used, which certainly
  // works, so it is good for debugging, but can overload those
  // frequencies, so be sure to configure the full frequency range of
  // your network here (unless your network autoconfigures them).
  // Setting up channels should happen after LMIC_setSession, as that
  // configures the minimal channel set.
  // NA-US channels 0-71 are configured automatically
  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
  // TTN defines an additional channel at 869.525Mhz using SF9 for class B
  // devices' ping slots. LMIC does not have an easy way to define set this
  // frequency and support for class B is spotty and untested, so this
  // frequency is not configured here.

  #elif defined(CFG_us915)
  // NA-US channels 0-71 are configured automatically
  // but only one group of 8 should (a subband) should be active
  // TTN recommends the second sub band, 1 in a zero based count.
  // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
  LMIC_selectSubBand(1);
  #endif

  // Enable the used channels
  setChannel(actconf.lchannel);

  // Disable link check validation
  LMIC_setLinkCheckMode(0);   // TODO: Enable again?

  // TTN uses SF9 for its RX2 window.
  LMIC.dn2Dr = DR_SF9;

  // Set spreading factor depends on transmit slot and transmit power for uplink
  // (note: txpow seems to be ignored by the library)
  setSF(slot, actconf.spreadf, actconf.dynsf);

  const bool wokeFromDeepSleep = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  const bool rtcConfigMatches = RTC_LMIC_CONFIG_FINGERPRINT == currentLoraConfigFingerprint();
  if (wokeFromDeepSleep && RTC_LMIC.seqnoUp != 0 && rtcConfigMatches)
  {
    LoadLMICFromRTC();
    DebugPrintln(3, "LMIC.seqnoUp restored from RTC: " + String(LMIC.seqnoUp));
  }
  else
  {
    if (RTC_LMIC.seqnoUp != 0 && !rtcConfigMatches) {
      DebugPrintln(2, "Ignoring RTC LMIC state because LoRa configuration changed");
    }
    DebugPrintln(3, "LMIC.seqnoUp: " + String(LMIC.seqnoUp));
    LMIC.seqnoUp = actconf.fcnt;
    DebugPrintln(3, "After LMIC.seqnoUp = actconf.fcnt: " + String(LMIC.seqnoUp));
  }

  DebugPrintln(3, "LMIC.seqnoUp: " + String(LMIC.seqnoUp));
  DebugPrintln(3, "actconf.fcnt: " + String(actconf.fcnt));

  DebugPrintln(3, "LoraWANDebug: ");
  LoraWANDebug(LMIC);
  // Sending is triggered by the active state only when LoRa mode allows it.
}

// copied from task.h
void lora_loop() {
  // Execute the LMIC scheduler
  os_runloop_once();
}

bool machine_state0_executeOnce = true;
bool machine_state1_executeOnce = true;
unsigned long lastWifiReconnectAttemptMillis = 0;
String activePowerOnLoraOperationMode = "";
bool powerOnLoraRuntimeActive = false;

bool isManualLoraSendBusy() {
  return manualLoraSendRequested || manualLoraSendInProgress;
}

bool isManualLoraKeepAwake() {
  return isManualLoraSendBusy() ||
         (manualLoraResultHoldUntilMillis > 0 && millis() < manualLoraResultHoldUntilMillis);
}

bool queueManualLoraSend(String &message) {
  const RemoteOtaSnapshot ota = getRemoteOtaSnapshot();
  if (localOtaInProgress || ota.pending || ota.inProgress || runDownloadingFiles || runDownloadingFilesStatus) {
    message = "An update task is active. Try the LoRa test again after it finishes.";
    setManualLoraStatus("error", message);
    return false;
  }
  if (!hasConfiguredLoraSession()) {
    message = "LoRa session is incomplete. Check Device Address, NwkSKey and AppSKey.";
    setManualLoraStatus("error", message);
    return false;
  }
  if (isManualLoraSendBusy()) {
    message = "A manual LoRa uplink is already queued or transmitting.";
    return false;
  }

  manualLoraRequestId++;
  manualLoraRequestedMillis = millis();
  manualLoraStartedMillis = 0;
  manualLoraCompletedMillis = 0;
  manualLoraLastAcknowledged = false;
  manualLoraLastDownlinkBytes = 0;
  manualLoraSendRequested = true;
  standbySleepBlockedUntilMillis = millis() + 90000UL;
  message = "Manual LoRa uplink queued.";
  setManualLoraStatus("queued", message);
  return true;
}

bool processManualLoraSend() {
  if (!isManualLoraSendBusy()) {
    return false;
  }

  const unsigned long now = millis();
  if (manualLoraSendRequested && now - manualLoraRequestedMillis > 75000UL) {
    failManualLoraSend("Manual LoRa request timed out while waiting for the radio.");
    return false;
  }
  if (manualLoraSendInProgress && now - manualLoraStartedMillis > 75000UL) {
    LMIC_clrTxData();
    os_clearCallback(&sendjob);
    failManualLoraSend("Manual LoRa transmission timed out after 75 seconds.");
    return false;
  }

  if (manualLoraSendRequested && !manualLoraSendInProgress) {
    if (!powerOnLoraRuntimeActive && !manualLoraRuntimeOwned) {
      setManualLoraStatus("preparing", "Initializing LoRa session for manual uplink.");
      lora_init();
      manualLoraRuntimeOwned = true;
    }

    if (LMIC.opmode & OP_TXRXPEND) {
      return true;
    }

    os_clearCallback(&sendjob);
    manualLoraSendRequested = false;
    manualLoraSendInProgress = true;
    manualLoraStartedMillis = millis();
    setManualLoraStatus("queued", "Reading sensors and queuing LoRa packet.");
    do_send(&sendjob);

    if (!(LMIC.opmode & OP_TXRXPEND)) {
      failManualLoraSend("LMIC did not accept the manual packet.");
    }
  }

  return isManualLoraSendBusy();
}

void noteManualLoraTxStarted() {
  if (!manualLoraSendInProgress) {
    return;
  }
  setManualLoraStatus("transmitting", "LoRa radio transmission started; waiting for RX windows.");
}

bool completeManualLoraSend(bool acknowledged, uint8_t downlinkBytes) {
  if (!manualLoraSendInProgress) {
    return false;
  }

  const bool standaloneRuntime = manualLoraRuntimeOwned;
  manualLoraSendInProgress = false;
  manualLoraRuntimeOwned = false;
  manualLoraCompletedMillis = millis();
  manualLoraResultHoldUntilMillis = millis() + 15000UL;
  manualLoraLastAcknowledged = acknowledged;
  manualLoraLastDownlinkBytes = downlinkBytes;
  String message = "Manual LoRa radio cycle completed";
  if (acknowledged) {
    message += " with acknowledgement";
  }
  if (downlinkBytes > 0) {
    message += "; downlink bytes: " + String(downlinkBytes);
  }
  message += ".";
  setManualLoraStatus("complete", message);
  if (standaloneRuntime) {
    GOTO_DEEPSLEEP = false;
  }
  return standaloneRuntime;
}

bool failManualLoraSend(const char *reason) {
  if (!manualLoraSendRequested && !manualLoraSendInProgress) {
    return false;
  }

  const bool standaloneRuntime = manualLoraRuntimeOwned;
  manualLoraSendRequested = false;
  manualLoraSendInProgress = false;
  manualLoraRuntimeOwned = false;
  manualLoraCompletedMillis = millis();
  manualLoraResultHoldUntilMillis = millis() + 15000UL;
  setManualLoraStatus("error", String(reason));
  if (standaloneRuntime) {
    GOTO_DEEPSLEEP = false;
  }
  return standaloneRuntime;
}

void buildManualLoraStatus(JsonDocument &response) {
  char state[sizeof(manualLoraState)];
  char message[sizeof(manualLoraMessage)];
  ManualLoraLogEntry logSnapshot[MANUAL_LORA_LOG_SIZE];
  uint8_t logCount;
  uint8_t logWriteIndex;

  portENTER_CRITICAL(&manualLoraStatusMux);
  strlcpy(state, manualLoraState, sizeof(state));
  strlcpy(message, manualLoraMessage, sizeof(message));
  memcpy(logSnapshot, manualLoraLog, sizeof(logSnapshot));
  logCount = manualLoraLogCount;
  logWriteIndex = manualLoraLogWriteIndex;
  portEXIT_CRITICAL(&manualLoraStatusMux);

  response["state"] = state;
  response["message"] = message;
  response["busy"] = isManualLoraSendBusy();
  response["requestId"] = manualLoraRequestId;
  response["requestedMillis"] = manualLoraRequestedMillis;
  response["startedMillis"] = manualLoraStartedMillis;
  response["completedMillis"] = manualLoraCompletedMillis;
  response["acknowledged"] = manualLoraLastAcknowledged;
  response["downlinkBytes"] = manualLoraLastDownlinkBytes;
  response["operationMode"] = String(actconf.loraOperationMode);
  response["txPending"] = (LMIC.opmode & OP_TXRXPEND) != 0;
  response["txCounter"] = LMIC.seqnoUp;
  response["channel"] = getLMICtxChnl();
  response["spreadingFactor"] = sf;
  response["uptimeMillis"] = millis();

  JsonArray events = response["events"].to<JsonArray>();
  const uint8_t firstIndex = (logWriteIndex + MANUAL_LORA_LOG_SIZE - logCount) % MANUAL_LORA_LOG_SIZE;
  for (uint8_t i = 0; i < logCount; i++) {
    const ManualLoraLogEntry &entry = logSnapshot[(firstIndex + i) % MANUAL_LORA_LOG_SIZE];
    JsonObject event = events.add<JsonObject>();
    event["millis"] = entry.timestampMillis;
    event["message"] = entry.message;
  }
}

void maintainAlwaysOnWifi() {
  const unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    ensureMdnsService();
    return;
  }

  if (now - lastWifiReconnectAttemptMillis < 15000UL) {
    return;
  }
  if ((LMIC.opmode & OP_TXRXPEND) || os_queryTimeCriticalJobs(ms2osticksRound(2000))) {
    return;
  }

  lastWifiReconnectAttemptMillis = now;
  DebugPrintln(2, "WiFi disconnected in always-on mode, reconnecting...");
  writeDisplayStatusScreen("WiFi", "Disconnected", "Reconnecting");
  enableWiFi();
}

void syncPowerOnLoraMode() {
  const String currentMode = String(actconf.loraOperationMode);
  if (currentMode == activePowerOnLoraOperationMode) {
    return;
  }

  activePowerOnLoraOperationMode = currentMode;
  if (!isLoraEnabledInPowerOn()) {
    Timer4.detach();
    GOTO_DEEPSLEEP = false;
    powerOnLoraRuntimeActive = false;
    DebugPrintln(3, "Power-on LoRa runtime disabled.");
    return;
  }

  Timer4.attach_ms((1000 * TX_INTERVAL), &onTimer);
  sendLoraQueue = true;
  lora_init();
  do_send(&sendjob);
  powerOnLoraRuntimeActive = true;
  DebugPrintln(3, "Power-on LoRa runtime enabled for mode: " + currentMode);
}

// S0 = Standby (Main sw off, WiFi off, Lora send every x minutes)
void state0(){
  if(machine_state0_executeOnce){
    DebugPrintln(3, " ");
    DebugPrintln(3, "state0 once");

    if (standbyWakeDisplayActive) {
      showStandbyWakeDisplay(pendingWakeReasonLabel,
                             String(actconf.SendDataViaWifi) == "Yes" ? "Mode: WiFi/MDS" : "Mode: LoRa",
                             "Battery switch");
    } else {
      writeDisplayStatusScreen("Standby mode", "Battery switch", "Mode: LoRa");
    }

	    const bool loraEnabledInStandby = isLoraEnabledInStandby();
    if (loraEnabledInStandby) {
      sendLoraQueue = true;
    }

	    const bool standbyMdsUploadEnabled = isMdsUploadEnabled();
    const bool standbyWifiServicesEnabled = (String(actconf.WifiStandbyMode) == "Yes");
    const bool standbyAutoUpdateEnabled = shouldEnableWifiForStandbyAutoUpdate();
    if (standbyWifiServicesEnabled || standbyMdsUploadEnabled || standbyAutoUpdateEnabled) {
      enableWiFi(false);
    }
    else {
      disableWiFiForSleep();
    }

    // In standby keep the relay off unless a timed downlink action turns it on.
    digitalWrite(relayPin, LOW);

    if (loraEnabledInStandby) {
      lora_init();
    }
    machine_state0_executeOnce = false;
    machine_state1_executeOnce = true;
    sendedLoraAfterSleepOneTime = false;
    delay(500);
  }

  lora_loop();
  delay(20);

	  const bool loraEnabledInStandby = isLoraEnabledInStandby();
	  const bool wifiUploadEnabled = isMdsUploadEnabled();
	  if (loraEnabledInStandby) {
	    if (!sendedLoraAfterSleepOneTime && wifiUploadEnabled && isWifiFirstTransmitPriority()) {
	      maybeSendDataViaWifi();
	    }
	    if (!sendedLoraAfterSleepOneTime) {
	      DebugPrintln(3, "do_send, in state0.");
	      do_send(&sendjob);
	      sendedLoraAfterSleepOneTime = true;
	    }

    static unsigned long lastPrintTime = 0;
    const bool timeCriticalJobs = os_queryTimeCriticalJobs(ms2osticksRound((TX_INTERVAL * 1000)));
    if (!timeCriticalJobs && GOTO_DEEPSLEEP == true && !(LMIC.opmode & OP_TXRXPEND)) {
      DebugPrintln(3, "Lora send done. (state0)");
	      DebugPrintln(3, "Can go sleep ");
	      LoraWANPrintLMICOpmode();
		      const uint32_t actualSleepSeconds = uint32_t(max(1, actconf.standbySleepDuration)) * 60UL;
		      SaveLMICToRTC(actualSleepSeconds);
	      if (wifiUploadEnabled && !isWifiFirstTransmitPriority()) {
	        maybeSendDataViaWifi();
	      }
	      processStandbyAutoUpdate();
	      delay(500); // give some time to save.
	      prepareForStandbySleep();
	      GoDeepSleep();
    } else if (lastPrintTime + 2000 < millis()) {
      if (standbyWakeDisplayActive) {
        showStandbyWakeDisplay("LoRa sending",
                               pendingWakeReasonLabel,
                               "Please wait");
      } else {
        if (toggleDisplayStatus) {
          u8x8.drawString(0,4,".");
          toggleDisplayStatus = false;
        } else {
          u8x8.drawString(0,4," ");
          toggleDisplayStatus = true;
        }
      }

      lastPrintTime = millis();
      unsigned long difference = (lastPrintTime - loraSendDurationTime) / 1000;
      DebugPrintln(3, "difference: " + String(difference));
      long seconds = millis() / 1000;
	      if (difference >= 50) {  // Abord sending, after 50 seconds
	        DebugPrintln(3, "seconds >= 50");
	        SaveLMICToRTC(TX_INTERVAL);
	        if (wifiUploadEnabled && !isWifiFirstTransmitPriority()) {
	          maybeSendDataViaWifi();
	        }
	        processStandbyAutoUpdate();
	        delay(500);
	        prepareForStandbySleep();
	        GoDeepSleep();
      }
    }
	  } else {
	    // Standby must still enter deep sleep even when LoRa is disabled.
	    if (wifiUploadEnabled) {
	      maybeSendDataViaWifi();
	    }
	    processStandbyAutoUpdate();
	    prepareForStandbySleep();
	    GoDeepSleep();
	  }

	  if (wifiUploadEnabled && (!loraEnabledInStandby || isWifiFirstTransmitPriority() || GOTO_DEEPSLEEP == false)) {
	    maybeSendDataViaWifi();
	  }
}

// S1 = Battery On (Wifi on)
void state1(){
  if(machine_state1_executeOnce){
    DebugPrintln(3, "state1 once");
    enableWiFi();
    lastWifiReconnectAttemptMillis = millis();
    delay(2000);    // to be able to read the displayed infos.
    if (standbyWakeDisplayActive) {
      showStandbyWakeDisplay(pendingWakeReasonLabel,
                             "WiFi active",
                             "Transferring");
    } else {
      u8x8.setPowerSave(0);
      u8x8.clearDisplay();
      u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.refreshDisplay();
    }

    if (actconf.relay == 1) {
      digitalWrite(relayPin, HIGH); // Relay on
    } else if (actconf.relay == 2) {
      digitalWrite(relayPin, HIGH); // Relay on
    }

		    if (isMdsUploadEnabled() && isWifiFirstTransmitPriority()) {
		      maybeSendDataViaWifi();
		    }
		    syncPowerOnLoraMode();
		    writeDisplayByMode(actconf);
    machine_state1_executeOnce = false;
    machine_state0_executeOnce = true;
    delay(500);
  }

	  syncPowerOnLoraMode();
	  if (powerOnLoraRuntimeActive) {
    lora_loop();
    static unsigned long lastPrintTime = 0;
    const bool timeCriticalJobs = os_queryTimeCriticalJobs(ms2osticksRound((TX_INTERVAL * 1000)));
    if (!timeCriticalJobs && GOTO_DEEPSLEEP == true && !(LMIC.opmode & OP_TXRXPEND)) {
      DebugPrintln(3, "Lora send done. (state1)");
      GOTO_DEEPSLEEP = false;
    }
  }
  maintainAlwaysOnWifi();
  //httpServer.handleClient();   // HTTP Server-handler for HTTP update server
  //old Voltage Offset: 6.47301

		  if (!powerOnLoraRuntimeActive || !(LMIC.opmode & OP_TXRXPEND)) {
	    maybeSendDataViaWifi();
	  }
  VEdirectSend();

  // Read measuring data and display on OLED all 1s
  static unsigned long lastMeasurementMillis = 0;
  if (millis() - lastMeasurementMillis >= 1000UL) {
    lastMeasurementMillis = millis();

    // BME280 measuerement
    if (String(actconf.envSensor) == "BME280") {
      bme.takeForcedMeasurement(); // has no effect in normal mode
    }
	    writeDisplayByMode(actconf);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.loop();
      //DebugPrintln(3, "Update Display.");
    }
    readValues(actconf);
  }
  VEdirectRead();
  
  // TCP-Server for NMEA0183
  WiFiClient client = server.available();// Check if a client is connected
  static unsigned long nmeaPacketCounter = 0;
  //client = server.available();// Check if a client is connected

  //DebugPrint(3, "client.connected(): " + String(client.connected()));
  //DebugPrintln(3, ", client.available(): " + String(client.available()));
  // Never wait for an idle TCP client here. LMIC, OTA and standby handling all
  // depend on returning to loop() within a few milliseconds.
  if (client.connected()) {
    //DebugPrint(3, "While client connected.");
    //httpServer.handleClient();      // HTTP Server-handler for HTTP update server

    // Sending XDR data
    if (flag1 == true){
      nmeaPacketCounter++;
      DebugPrintln(3, "");
      DebugPrint(3, "Send package:");
      DebugPrintln(3, nmeaPacketCounter);

      if((int(actconf.serverMode) == 0) || (int(actconf.serverMode) == 1) || (int(actconf.serverMode) == 4)){
         if(int(actconf.senddata) == 1){
            if (String(actconf.envSensor) == "BME280") {
              client.println(sendXDR1(1));  // Send XDR1 telegram environment sensors
            }
            client.println(sendXDR2(1));    // Send XDR2 telegram battery sensors
            client.println(sendXDR3(1));    // Send XDR3 telegram level and control
            if(nmea != ""){
              client.println(sendRMC(1));   // Send GPS RMC telegram
            }
            client.println("$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74");
            client.println("$GPGLL,4738.9884,N,12226.9827,W,220259,A*32");
            /* $GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00*74 $GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00,,,,*4D $GPGLL,4738.9884,N,12226.9827,W,220259,A*32 $GPGGA,220259,4738.9884,N,12226.9827,W,1,12,1.5,0.0,M,0.0,M,*41 $GPVHW,45.0,T,63.0,M,0.0,N,0.0,K*46 $GPVDR,135.0,T,149.0,M,2.2,N*25 $HCHDT,45.0,T*18 $HCHDM,63.0,M*1C $IIMWV,316,T,0.0,N,A*21 $IIMWV,NaN,R,0.0,N,A*72 $GPRMC,220259,A,4738.9884,N,12226.9827,W,2.2,135.0,220523,14.0,E*58");*/
         }
      }
      flag1 = false;                        // Reset the send flag
      //break;
    }
  }

  //___________________________

  //--------------------------------------------------------
  // Assembly and sending of the NMEA0183-sentence via TCP
  //--------------------------------------------------------
  
  /*if (server.hasClient()) {  //only send if there are clients to receive
    for (int i = 0; i < MAX_CLIENTS; i++) {

      //added check for clients[i].status==0 to reuse connections
      if ( !(clients[i] && clients[i].connected() ) ) {
        if (clients[i]) {
          clients[i].stop(); // make room for new connection
        }
        clients[i] = server.available();
        continue;
      }
    }
     // No free spot or exceeded MAX_CLENTS so reject incoming connection
    server.available().stop();
  }

  //checksum(MWVSentence).toCharArray(g, 85); //calculate checksum and store it

  //final assembly of the TCP-message to be send
  /*strcpy(result, "$");  //start with the dollar symbol
  strcat(result, MWVSentence); //append the MWVSentence
  strcat(result, "*"); //star-seperator for the CS
  strcat(result, g);  //append the CS*/

//  strcpy(result, "$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74");    // Send XDR2 telegram battery sensors
  //client.println(sendXDR3(1));    // Send XDR3 telegram level and control

  // Broadcast NMEA0183 sentence to all clients
/*  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] && clients[i].connected()) {
      clients[i].println(result);  //make sure to use println and not write, at least it did not work for me
      //clients[i].println("$");
      //clients[i].println("$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74");
      //$GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00*74 $GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00,,,,*4D $GPGLL,4738.7636,N,12226.6491,W,215915,A*30 $GPGGA,215915,4738.7636,N,12226.6491,W,1,12,1.5,0.0,M,0.0,M,*43 $GPVHW,45.0,T,63.0,M,0.0,N,0.0,K*46 $GPVDR,135.0,T,149.0,M,2.2,N*25 $HCHDT,45.0,T*18 $HCHDM,63.0,M*1C $IIMWV,313,T,0.0,N,A*24 $IIMWV,NaN,R,0.0,N,A*72 $GPRMC,215915,A,4738.7636,N,12226.6491,W,2.2,135.0,230523,14.0,E*5B ");
    }
  }*/
  //___________________________

  if (Serial2.available() && !lora_activ && !loraEvent_activ) {
    readGPSValues();
  }

  static unsigned long nextWebFilesDownloadAttempt = 0;
  if (runDownloadingFiles && WiFi.status() != WL_CONNECTED) {
    setWebFilesUpdateMessage("Waiting for WiFi connection before downloading web files.");
    writeDisplayStatusScreen("Web files", "Waiting WiFi", "before update");
    const WebFilesUpdateSnapshot state = getWebFilesUpdateSnapshot();
    if (state.startedMillis > 0 && millis() - state.startedMillis > 60000UL) {
      runDownloadingFiles = false;
      runDownloadingFilesStatus = false;
      setWebFilesUpdateError(true, "Web files download cancelled because WiFi is not connected.");
      writeDisplayStatusScreen("Web files", "Download canceled", "No WiFi");
    }
  }

  if (runDownloadingFiles && WiFi.status() == WL_CONNECTED && millis() >= nextWebFilesDownloadAttempt) {
    runDownloadingFilesStatus = true;
    setWebFilesUpdateError(false, "Downloading web files from server.");
    const bool webFilesCurrent = DownloadFilesFromWeb();
    if (webFilesCurrent) {
      runDownloadingFiles = false;
      setWebFilesUpdateRetryCount(0);
      setWebFilesUpdateMessage("Web files updated successfully.");
      writeDisplayStatusScreen("Web files", "Update complete", String(actconf.fversion));
      nextWebFilesDownloadAttempt = 0;
    } else {
      const WebFilesUpdateSnapshot state = getWebFilesUpdateSnapshot();
      const String lastFailureReason = state.message;
      const uint8_t retryCount = incrementWebFilesUpdateRetryCount();
      if (state.fatalError || retryCount >= 3) {
        runDownloadingFiles = false;
        setWebFilesUpdateError(true, state.fatalError ? lastFailureReason : "Web files download failed. Last error: " + lastFailureReason);
        writeDisplayStatusScreen("Web files", "Download failed", "Retry manual");
        nextWebFilesDownloadAttempt = 0;
      } else {
        setWebFilesUpdateProgress(0, 0, "", "Web files download failed. Retrying shortly. Last error: " + lastFailureReason);
        writeDisplayStatusScreen("Web files", "Retry shortly", "In 30 seconds");
        nextWebFilesDownloadAttempt = millis() + 30000UL;
      }
    }
    runDownloadingFilesStatus = false;
    finishDisplayProgressMode(webFilesCurrent ? 5000UL : 10000UL);
  }

  processPendingRemoteOta("Remote", false);

  if(reboot && scheduledRestartMillis == 0){
    DebugPrintln(3, "Reboot");
    ESP.restart(); // Restart ESP32
  }
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
const char* wakeupReasonToLabel(esp_sleep_wakeup_cause_t wakeup_reason) {
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: return "Wakeup EXT0";
    case ESP_SLEEP_WAKEUP_EXT1: return "Wakeup EXT1";
    case ESP_SLEEP_WAKEUP_TIMER: return "Wakeup Timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Wakeup Touch";
    case ESP_SLEEP_WAKEUP_ULP: return "Wakeup ULP";
    default: return "Wakeup Other";
  }
}

const char* resetReasonToLabel(esp_reset_reason_t reset_reason) {
  switch (reset_reason) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "external reset";
    case ESP_RST_SW: return "software reset";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  esp_reset_reason_t reset_reason = esp_reset_reason();
  DebugPrintln(3, "Reset reason: " + String(resetReasonToLabel(reset_reason)) + " (" + String(static_cast<int>(reset_reason)) + ")");
  pendingWakeReasonCode = static_cast<int>(wakeup_reason);
  pendingWakeReasonLabel = wakeupReasonToLabel(wakeup_reason);
  pendingWakeMdsEvent = isDeepSleepWakeup(wakeup_reason);
  standbyWakeDisplayActive = pendingWakeMdsEvent;
  standbyWakeDisplayHoldUntilMillis = standbyWakeDisplayActive ? (millis() + STANDBY_WAKE_DISPLAY_HOLD_MS) : 0;
  currentWakeMdsEventCaptured = false;
  currentWakeupBootMillisCaptured = false;
  nextWakeMdsRetryMillis = 0;
  if (standbyWakeDisplayActive) {
    writeDisplayStatusScreen("Standby wake",
                             pendingWakeReasonLabel,
                             "Device woke up",
                             "Preparing...");
  }
  if (pendingWakeMdsEvent && hasValidWakeSleepTime()) {
    markWakeupBootMoment(pendingWakeReasonLabel.c_str());
    registerWakeupEvent(pendingWakeReasonLabel.c_str());
    DebugPrintln(3, "Wakeup timestamp captured immediately at boot: " + String(static_cast<unsigned long>(lastWakeupEventEpoch)));
  } else if (pendingWakeMdsEvent) {
    markWakeupBootMoment(pendingWakeReasonLabel.c_str());
    DebugPrintln(2, "Wakeup detected, but reliable system time is not available yet. Timestamp capture will be retried later.");
  }
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : DebugPrintln(3, "Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : DebugPrintln(3, "Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : DebugPrintln(3, "Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : DebugPrintln(3, "Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : DebugPrintln(3, "Wakeup caused by ULP program"); break;
    default : DebugPrintln(3, "Wakeup was not caused by deep sleep: " + String(wakeup_reason)); break;
  }
}

bool isPrintableConfigString(const char *value, size_t maxLength) {
  if (value == nullptr) {
    return false;
  }

  for (size_t i = 0; i < maxLength; i++) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c == '\0') {
      return true;
    }
    if (c < 32 || c > 126) {
      return false;
    }
  }

  return false;
}

bool isValidHttpsUrlString(const char *value, size_t maxLength) {
  if (!isPrintableConfigString(value, maxLength)) {
    return false;
  }

  String url = String(value);
  url.trim();
  return url.startsWith("https://") && url.length() > 8 && url.length() < maxLength;
}

bool copyConfigString(char *target, size_t targetLength, const char *source) {
  if (target == nullptr || targetLength == 0 || source == nullptr) {
    return false;
  }

  if (strncmp(target, source, targetLength) == 0) {
    return false;
  }

  strncpy(target, source, targetLength - 1);
  target[targetLength - 1] = '\0';
  return true;
}

bool sanitizeConfigOption(char *target, size_t targetLength, const char *fallback, const char * const *allowedValues, size_t allowedCount) {
  if (!isPrintableConfigString(target, targetLength)) {
    return copyConfigString(target, targetLength, fallback);
  }

  for (size_t i = 0; i < allowedCount; i++) {
    if (strcmp(target, allowedValues[i]) == 0) {
      return false;
    }
  }

  return copyConfigString(target, targetLength, fallback);
}

bool repairKnownMdsUrl(char *target, size_t targetLength) {
  if (!isPrintableConfigString(target, targetLength)) {
    return copyConfigString(target, targetLength, defconf.MdsUrl);
  }

  String repaired = String(target);
  repaired.trim();

  if (repaired.length() == 0) {
    return false;
  }

  if (repaired.indexOf("://") < 0) {
    repaired = "https://" + repaired;
  }

  // Repair known corrupted/legacy hosts without touching custom user endpoints.
  repaired.replace("https://s-git.derguntmar.de/", "https://mds-git.derguntmar.de/");
  repaired.replace("https://git.derguntmar.de/", "https://mds-git.derguntmar.de/");

  if (repaired == "https://mds-git.derguntmar.de" || repaired == "https://mds-git.derguntmar.de/") {
    repaired = String(defconf.MdsUrl);
  }

  if (repaired == String(target)) {
    return false;
  }

  if (repaired.length() >= targetLength) {
    return copyConfigString(target, targetLength, defconf.MdsUrl);
  }

  repaired.toCharArray(target, targetLength);
  return true;
}

template <typename T>
bool sanitizeConfigRange(T &value, T fallback, T minValue, T maxValue) {
  if (value >= minValue && value <= maxValue) {
    return false;
  }

  value = fallback;
  return true;
}

void sanitizeNewConfigFields() {
  bool changed = false;

  changed = sanitizeConfigRange(actconf.timeout, defconf.timeout, 3, 30) || changed;
  changed = sanitizeConfigRange(actconf.apchannel, defconf.apchannel, 1, 13) || changed;
  changed = sanitizeConfigRange(actconf.maxconnections, defconf.maxconnections, 1, 4) || changed;
  changed = sanitizeConfigRange(actconf.mDNS, defconf.mDNS, 0, 1) || changed;
  if (actconf.dataport != defconf.dataport) {
    actconf.dataport = defconf.dataport;
    changed = true;
  }
  if (actconf.httpport != defconf.httpport) {
    actconf.httpport = defconf.httpport;
    changed = true;
  }
  changed = sanitizeConfigRange(actconf.serverMode, defconf.serverMode, 0, 4) || changed;
  changed = sanitizeConfigRange(actconf.serspeed, defconf.serspeed, 300, 115200) || changed;
  changed = sanitizeConfigRange(actconf.WebSerialDebug, defconf.WebSerialDebug, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.deviceID, defconf.deviceID, 0, 9) || changed;
  changed = sanitizeConfigRange(actconf.corder1, defconf.corder1, 0, 3) || changed;
  changed = sanitizeConfigRange(actconf.corder2, defconf.corder2, 0, 3) || changed;
  changed = sanitizeConfigRange(actconf.corder3, defconf.corder3, 0, 3) || changed;
  changed = sanitizeConfigRange(actconf.senddata, defconf.senddata, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.vaverage, defconf.vaverage, 1, 100) || changed;
  changed = sanitizeConfigRange(actconf.t1average, defconf.t1average, 1, 100) || changed;
  changed = sanitizeConfigRange(actconf.t2average, defconf.t2average, 1, 100) || changed;
  changed = sanitizeConfigRange(actconf.lchannel, defconf.lchannel, 0, 9) || changed;
  changed = sanitizeConfigRange(actconf.spreadf, defconf.spreadf, 7, 10) || changed;
  changed = sanitizeConfigRange(actconf.dynsf, defconf.dynsf, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.tinterval, defconf.tinterval, 1U, 255U) || changed;
  changed = sanitizeConfigRange(actconf.relay, defconf.relay, 0, 2) || changed;
  changed = sanitizeConfigRange(actconf.instrumentSize, defconf.instrumentSize, 200, 600) || changed;
  changed = sanitizeConfigRange(actconf.skin, defconf.skin, 0, 2) || changed;
  changed = sanitizeConfigRange(actconf.standbySleepDuration, defconf.standbySleepDuration, 1, 1440) || changed;
  changed = sanitizeConfigRange(actconf.standbyAutoUpdateIntervalHours, defconf.standbyAutoUpdateIntervalHours, 1, 720) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdBattery, defconf.MdsSensorIdBattery, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdTanks, defconf.MdsSensorIdTanks, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdStatus, defconf.MdsSensorIdStatus, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdGps, defconf.MdsSensorIdGps, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdEnv, defconf.MdsSensorIdEnv, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdDewpoint, defconf.MdsSensorIdDewpoint, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdVedirect, defconf.MdsSensorIdVedirect, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.cssStyle, defconf.cssStyle, 0, 2) || changed;
  changed = sanitizeConfigRange(actconf.OledDisplayRotation, defconf.OledDisplayRotation, 0, 1) || changed;

  if (sanitizeBatteryCalibration(actconf, defconf)) {
    changed = true;
    DebugPrintln(2, "Battery calibration reset to safe defaults");
  }

  const char *yesNoValues[] = {"Yes", "No"};
  const char *tempSensorValues[] = {"Off", "DS18B20"};
  const char *tempUnitValues[] = {"C", "F"};
  const char *envSensorValues[] = {"Off", "BME280", "VEdirect-Read", "VEdirect-Send"};
  const char *standbyModeValues[] = {"Off", "On"};
  const char *loraModeValues[] = {"Off", "Standby", "PowerOn", "Always"};
  const char *transmitPriorityValues[] = {"LoRaFirst", "WifiFirst"};
  const char *oledDisplayModeValues[] = {"Values", "Status"};
  const char *loraFrequencyValues[] = {"EU868", "US915"};

  changed = sanitizeConfigOption(actconf.SendDataViaWifi, sizeof(actconf.SendDataViaWifi), defconf.SendDataViaWifi, yesNoValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.WifiStandbyMode, sizeof(actconf.WifiStandbyMode), defconf.WifiStandbyMode, yesNoValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.standbyAutoUpdate, sizeof(actconf.standbyAutoUpdate), defconf.standbyAutoUpdate, yesNoValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.tempSensorType, sizeof(actconf.tempSensorType), defconf.tempSensorType, tempSensorValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.tempUnit, sizeof(actconf.tempUnit), defconf.tempUnit, tempUnitValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.envSensor, sizeof(actconf.envSensor), defconf.envSensor, envSensorValues, 4) || changed;
  changed = sanitizeConfigOption(actconf.standbyMode, sizeof(actconf.standbyMode), defconf.standbyMode, standbyModeValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.loraOperationMode, sizeof(actconf.loraOperationMode), defconf.loraOperationMode, loraModeValues, 4) || changed;
  changed = sanitizeConfigOption(actconf.transmitPriority, sizeof(actconf.transmitPriority), defconf.transmitPriority, transmitPriorityValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.OledDisplayMode, sizeof(actconf.OledDisplayMode), defconf.OledDisplayMode, oledDisplayModeValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.lorafrequency, sizeof(actconf.lorafrequency), defconf.lorafrequency, loraFrequencyValues, 2) || changed;

  if (repairKnownMdsUrl(actconf.MdsUrl, sizeof(actconf.MdsUrl))) {
    changed = true;
    DebugPrintln(2, "MDS URL repaired");
  }

  if (!isValidHttpsUrlString(actconf.MdsUrl, sizeof(actconf.MdsUrl))) {
    if (isPrintableConfigString(actconf.MdsUrl, sizeof(actconf.MdsUrl)) && strlen(actconf.MdsUrl) > 0 && strstr(actconf.MdsUrl, "://") == nullptr) {
      String normalizedMdsUrl = "https://" + String(actconf.MdsUrl);
      if (normalizedMdsUrl.length() < sizeof(actconf.MdsUrl)) {
        normalizedMdsUrl.toCharArray(actconf.MdsUrl, sizeof(actconf.MdsUrl));
        changed = true;
      }
    }
    if (!isValidHttpsUrlString(actconf.MdsUrl, sizeof(actconf.MdsUrl))) {
      changed = copyConfigString(actconf.MdsUrl, sizeof(actconf.MdsUrl), defconf.MdsUrl) || changed;
    }
  }

  if (actconf.firmwareUpdateUrl[0] != '\0') {
    actconf.firmwareUpdateUrl[0] = '\0';
    changed = true;
    DebugPrintln(2, "Legacy web update host cleared; web files now derive from the MDS OTA endpoint");
  }

  if (!isValidHttpsUrlString(actconf.mdsOtaUrl, sizeof(actconf.mdsOtaUrl))) {
    strncpy(actconf.mdsOtaUrl, defconf.mdsOtaUrl, sizeof(actconf.mdsOtaUrl) - 1);
    actconf.mdsOtaUrl[sizeof(actconf.mdsOtaUrl) - 1] = '\0';
    changed = true;
    DebugPrintln(2, "Invalid MDS OTA endpoint reset to default");
  }

  if (!isPrintableConfigString(actconf.mdsOtaSecret, sizeof(actconf.mdsOtaSecret))) {
    actconf.mdsOtaSecret[0] = '\0';
    changed = true;
    DebugPrintln(2, "Invalid MDS OTA secret cleared");
  }

  if (changed) {
    saveEEPROMConfig(actconf);
  }
}

void setup() {
  initializeUpdateRuntimeState();
  LegacyConfigDataV16 legacyConfig;
  const bool hasLegacyConfig = readLegacyConfigFromEeprom(legacyConfig);
  if (hasLegacyConfig) {
    if (repairLegacyConfigV15(legacyConfig)) {
      DebugPrintln(2, "Repaired config layout from version 15");
    }
    actconf = migrateLegacyConfigV16ToCurrent(legacyConfig);
    saveEEPROMConfig(actconf);
    empty = 0;
  } else {
    actconf = loadEEPROMConfig(); // Overload with old EEPROM configuration by start. It is necessarry for serspeed
  }

  if (actconf.valid == defconf.valid) {
    empty = 0;                  // Marker for configuration is present
    if (!hasEEPROMConfigHeader()) {
      saveEEPROMConfig(actconf); // Migrate legacy raw layout to headered layout
    }
  }
  else{
    saveEEPROMConfig(defconf);
    actconf = defconf;
    empty = 1;                  // Marker for configuration is missing
  }

  sanitizeNewConfigFields();

  // Rebuild network servers with the persisted ports before first use.
  // The network server instances are global objects. Reconstructing them with
  // placement-new after static initialization corrupts internal AsyncWebServer
  // state and can crash as soon as TCP traffic starts.

  // If the firmware version in EEPROM is different to the default config,
  // only update the stored version string and preserve the rest of the config.
  if(strcmp(actconf.fversion, defconf.fversion) != 0){
    String fver = defconf.fversion;
    fver.toCharArray(actconf.fversion, sizeof(actconf.fversion));
    saveEEPROMConfig(actconf);
  }

  /* Attach Message Callback */
  WebSerial.onMessage([&](uint8_t *data, size_t len) {
    if (actconf.WebSerialDebug != 1) {
      return;
    }

    DebugPrintln(3, "Received " + String(len) + " bytes from WebSerial: ");
    String d = "";
    for(size_t i=0; i < len; i++){
      d += char(data[i]);
    }

    DebugPrintln(3, "Received Data: " + String(d));
  });

  //##### Start OLED #####
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(actconf.OledDisplayRotation);

  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0,0,"Starting...");
  u8x8.refreshDisplay();    // Only required for SSD1606/7  

  //##### Start serial 0 and serial 2 connections #####
  Serial.begin(actconf.serspeed);               // NMEA0183 an debug messages
  delay(200);
  if(String(actconf.envSensor) == "VEdirect-Read" || String(actconf.envSensor) == "VEdirect-Send"){
    Serial1.begin(19200, SERIAL_8N1, RXD1, TXD1); // VE.direct Victron interface (read and write)
  }
  //delay(200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // GPS (NMEA0183)
  //delay(200);

  macAddress = ESP.getEfuseMac();
  macAddressTrunc = macAddress << 40;
  chipId = macAddressTrunc >> 40;

  bool credentialsChanged = false;
  const String generatedWebPassword = buildDefaultWebPassword();
  const bool shouldRefreshGeneratedWebPassword =
    isGeneratedWebPassword(actconf.password) ||
    actconf.password[0] == '\0' ||
    strcmp(actconf.password, "12345678") == 0 ||
    strcmp(actconf.password, "auto-generated") == 0;
  if (shouldRefreshGeneratedWebPassword && generatedWebPassword.length() > 0) {
    generatedWebPassword.toCharArray(actconf.password, sizeof(actconf.password));
    actconf.crypt = 1;
    credentialsChanged = true;
    DebugPrintln(2, "Default web credentials initialized for user admin");
  } else if (actconf.crypt != 1) {
    actconf.crypt = 1;
    credentialsChanged = true;
    DebugPrintln(2, "Web authentication enabled for existing configuration");
  }

  const String generatedApPassword = buildDefaultApPassword();
  const bool shouldRefreshGeneratedApPassword =
    isGeneratedApPassword(actconf.spassword) ||
    actconf.spassword[0] == '\0' ||
    strcmp(actconf.spassword, "12345678") == 0 ||
    strcmp(actconf.spassword, "auto-generated") == 0;
  if (shouldRefreshGeneratedApPassword && generatedApPassword.length() > 0) {
    generatedApPassword.toCharArray(actconf.spassword, sizeof(actconf.spassword));
    credentialsChanged = true;
    DebugPrintln(2, "Default access point credentials initialized");
  }

  if (credentialsChanged) {
    saveEEPROMConfig(actconf);
  }

  //##### Start OLED #####
  u8x8.clearDisplay();
  u8x8.drawString(0,0,"LoRa Monitor");
  u8x8.drawString(0,1,"Starting up");
  u8x8.drawString(10,2,actconf.fversion);
  u8x8.drawString(0,4,"WiFi target:");
  //u8x8.drawString(0,4,actconf.cssid1);
  u8x8.refreshDisplay();    // Only required for SSD1606/7

  // ESP32 Information Data
  DebugPrintln(3, "******************************************");
  DebugPrintln(3, "******************************************");
  DebugPrintln(3, "Booting Sketch...");
  DebugPrintln(3, "");
  DebugPrint(3, actconf.devname);
  DebugPrint(3, " ");
  DebugPrint(3, actconf.fversion);
  DebugPrintln(3, " (C) Norbert Walter and modified by Guntmar Hoeche (2023)");
  DebugPrintln(3, "******************************************");
  DebugPrintln(3, "");
  DebugPrintln(3, "Modul Type: Heltec LoRa-32");
  DebugPrint(3, "SDK-Version: ");
  DebugPrint(3, ESP.getSdkVersion());
  DebugPrint(3, ", ESP32 Chip-ID: ");
  DebugPrint(3, chipId);
  DebugPrint(3, ", ESP32 Speed [MHz]: ");
  DebugPrint(3, ESP.getCpuFreqMHz());
  DebugPrint(3, ", Free Heap Size [Bytes]: ");
  DebugPrintln(3, ESP.getFreeHeap());
  DebugPrintln(3, "");

  DebugPrint(3, "Config Size [Bytes] (max is 2kB - 32B): ");
  DebugPrintln(3, sizeof(actconf));

  // Debug info for initialize the EEPROM
  if(empty == 1){
    DebugPrintln(3, "EEPROM config missing, initialization done");
  }
  else{
    DebugPrintln(3, "EEPROM config present");
  }

  // EEPROM config Version
  DebugPrintln(3, "actconf.fversion: " + String(actconf.fversion));
  DebugPrintln(3, "defconf.fversion: " + String(defconf.fversion));

  DebugPrint(3, "Sensor ID: ");
  DebugPrintln(3, actconf.deviceID);
  DebugPrintln(3, "Sensor Type: LoRa1000");
  DebugPrintln(3, "Info: LoRa Boat Monitor");
  DebugPrintln(3, "Voltage Input [V]: 0...12 ");
  DebugPrint(3, "Input Pin: GPIO ");
  DebugPrintln(3, ANALOG_IN);
  DebugPrintln(3, "Tank1 [%]: 0...100");
  DebugPrint(3, "Input Pin: GPIO ");
  DebugPrintln(3, TANK1_IN);
  DebugPrintln(3, "Tank2 [%]: 0...100");
  DebugPrint(3, "Input Pin: GPIO ");
  DebugPrintln(3, TANK2_IN);
  DebugPrintln(3, "Temp Sensor: SD18B20 1Wire");
  DebugPrintln(3, "Value Range [°C]: -55...125");
  DebugPrint(3, "Input Pin: GPIO ");
  DebugPrintln(3, OneWIRE_PIN);
  DebugPrint(3, "Temp Unit: ");
  DebugPrintln(3, actconf.tempUnit);
  DebugPrintln(3, "");
  if (String(actconf.envSensor) == "Off") {
    DebugPrintln(3, "Env Sensor: Off");
  }
  if (String(actconf.envSensor) == "BME280") {
    DebugPrintln(3, "Env Sensor: BME280");
  }
  if (String(actconf.envSensor) == "VEdirect-Read") {
    DebugPrintln(3, "Env Sensor: VEdirect reading");
  }
  if (String(actconf.envSensor) == "VEdirect-Send") {
    DebugPrintln(3, "Env Sensor: VE.direct sending");
  }
  DebugPrintln(3,"Serial0 Txd is on pin: "+String(TX));
  DebugPrintln(3,"Serial0 Rxd is on pin: "+String(RX));
  DebugPrintln(3,"Serial1 Txd is on pin: "+String(TXD1));
  DebugPrintln(3,"Serial1 Rxd is on pin: "+String(RXD1));
  DebugPrintln(3,"Serial2 Txd is on pin: "+String(TXD2));
  DebugPrintln(3,"Serial2 Rxd is on pin: "+String(RXD2));
  DebugPrintln(3, "");

  DebugPrint(3, "LoRa Frequency: ");
  DebugPrintln(3, actconf.lorafrequency);
  DebugPrint(3, "LoRa Channel: ");
  DebugPrintln(3, actconf.lchannel);
  DebugPrint(3, "Send Period [x60s]: ");
  DebugPrintln(3, actconf.tinterval);
  DebugPrint(3, "Spreading Factor: ");
  DebugPrintln(3, actconf.spreadf);
  DebugPrint(3, "Dynamic SF: ");
  DebugPrintln(3, actconf.dynsf);
  DebugPrintln(3, "");


  //##### Pin Settings #####
  pinMode(ledPin, OUTPUT);          // LED Pin output
  pinMode(relayPin, OUTPUT);        // Relay Pin output
  pinMode(alarmPin, INPUT);         // Alarm Pin input (external circuit provides the level)
  configureAnalogInputs();

  //##### Start 1Wire sensors #####
  if (String(actconf.tempSensorType) == "DS18B20") {
    sensors.begin();
  }  

  //##### Cyclic timer #####
  //Timer2.attach_ms(300000, relayTimerInterrupt);      // Start timer 2 all 5min cyclic counter increment
  //Timer3.attach_ms(SendPeriod, sendNMEA);    // Data transmission timer for NMEA

  //####### Starting BME280 ######
  if (String(actconf.envSensor) == "BME280") {
    DebugPrint(3, "BME280 test at address: ");
    DebugPrintln(3, "0x"+String(address, HEX));
    I2CBME.begin(I2C_SDA, I2C_SCL, I2C_SPEED); // Redefinition of I2C pins see Definition.h

    //  if (! bme.begin(address, &Wire)) { // Standard using I2C
    if (! bme.begin(address, &I2CBME)) {
      DebugPrintln(3,"Could not find a valid BME280 sensor, check wiring!");
      //actconf.envSensor = "BME280";

      u8x8.drawString(0,6,"BME280 not");
      u8x8.drawString(0,7,"detected");
      u8x8.refreshDisplay();    // Only required for SSD1606/7
      delay(2000);

      /*u8x8.setFont(u8x8_font_chroma48medium8_r);
      u8x8.drawString(0,0,"NoWa(C)OBP");
      u8x8.drawString(11,0,actconf.fversion);
      u8x8.drawString(0,1,"Could not find a");
      u8x8.drawString(0,2,"valid BME280!");
      u8x8.drawString(0,4,"System stop");
      u8x8.refreshDisplay();    // Only required for SSD1606/7  
    } else {
      DebugPrint(3, "BME280 found at address: ");
      DebugPrintln(3, "0x"+String(address, HEX));
      DebugPrintln(3, "");
      
      // For more details on the following scenarious, see chapter
      // 3.5 "Recommended modes of operation" in the datasheet
      bme.setSampling(Adafruit_BME280::MODE_FORCED,   // Mode [NORMAL|FORCED|SLEEP]
                      Adafruit_BME280::SAMPLING_X2,   // Temperature [NONE|X2|X4|X8|X16]
                      Adafruit_BME280::SAMPLING_X16,  // Pressure [NONE|X2|X4|X8|X16]
                      Adafruit_BME280::SAMPLING_X1,   // Humidity [NONE|X2|X4|X8|X16]
                      Adafruit_BME280::FILTER_OFF     // Filter [OFF|X1...X16]
    //                  Adafruit_BME280::STANDBY_MS_0_5 ) //Only used in Normal Mode 0,5ms stand by time
                      );*/
    }
  }
  //delay(3000);
  //u8x8.clearDisplay();

  //####### Starting LoRaWAN ######
  DebugPrintln(3,"Starting LoRaWAN");
  DebugPrint(3, "LoRa Frequency: ");
  DebugPrintln(3, actconf.lorafrequency);
  DebugPrint(3, "LoRa Channel: ");
  DebugPrintln(3, actconf.lchannel);
  DebugPrint(3, "Send Period [x60s]: ");
  DebugPrintln(3, actconf.tinterval);
  DebugPrint(3, "Spreading Factor: ");
  DebugPrintln(3, actconf.spreadf);
  DebugPrint(3, "Dynamic SF: ");
  DebugPrintln(3, actconf.dynsf);
  /*DebugPrint(3, "Device Adr: ");
  DebugPrintln(3, actconf.devaddr);
  DebugPrint(3, "nwkskey: ");
  DebugPrintln(3, actconf.nskey);
  DebugPrint(3, "appskey: ");
  DebugPrintln(3, actconf.appkey);*/
  DebugPrintln(3, "");

  #ifdef VCC_ENABLE
  // For Pinoccio Scout boards
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);
  delay(1000);
  #endif

  // Set send interval
  TX_INTERVAL = actconf.tinterval * 60;

  // sleep config...
  //Increment boot number and print it every reboot
  ++bootCount;
  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Return the standby wake pin to normal runtime mode after boot.
  // The RTC wake configuration is armed again right before deep sleep.
  rtc_gpio_deinit(GPIO_NUM_39);

  readValues(actconf);     // initial read after boot, to get the status of alarm pin.
  DebugPrintln(3, "Standby input GPIO " + String(alarmPin) + " raw: " + String(digitalRead(alarmPin) == LOW ? "LOW" : "HIGH") + ", state: " + String(alarm1 ? "Active" : "Inactive") + " (active LOW)");
  DebugPrintln(3, "MDS upload config: SendDataViaWifi=" + String(actconf.SendDataViaWifi) +
                    ", url=" + String(strlen(actconf.MdsUrl) > 0 ? "set" : "missing") +
                    ", apiKey=" + String(strlen(actconf.MdsApiKey) > 0 ? "set" : "missing") +
                    ", sensors[B/T/S/G/E/D/V]=" +
                    String(actconf.MdsSensorIdBattery) + "/" +
                    String(actconf.MdsSensorIdTanks) + "/" +
                    String(actconf.MdsSensorIdStatus) + "/" +
                    String(actconf.MdsSensorIdGps) + "/" +
                    String(actconf.MdsSensorIdEnv) + "/" +
                    String(actconf.MdsSensorIdDewpoint) + "/" +
                    String(actconf.MdsSensorIdVedirect));

  keepAwakeAfterUpdateRestart = false;

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    DebugPrintln(1, "LittleFS Mount Failed");
    return;
  }

  cleanupStaleWebBundleArtifacts();

  if (!areWebFilesCurrent(actconf.fversion)) {
    DebugPrintln(2, "Web files version mismatch detected. Use 'Get Files from Server' in the File Manager to update them.");
  }

  sendedLoraAfterSleepOneTime = false;
}

void loop() {
  maintainLocalOtaUpload();

  if (scheduledRestartMillis > 0 && millis() >= scheduledRestartMillis) {
    Serial.println("Scheduled restart now");
    Serial.flush();
    ESP.restart();
  }

  if (localOtaInProgress) {
    delay(20);
    return;
  }

  if (processPendingRemoteOta("Remote", false)) {
    delay(20);
    return;
  }

  if (processManualLoraSend()) {
    lora_loop();
    delay(20);
    return;
  }

  if ((alarm1 == true) || (String(actconf.standbyMode) == "Off")) {
    state1();
  }

  if ((alarm1 == false) && (String(actconf.standbyMode) == "On")) {
    if (millis() < standbySleepBlockedUntilMillis && hasActiveStandbyKeepAwakeWork()) {
      state1();
    } else {
      if (standbySleepBlockedUntilMillis > 0 && millis() < standbySleepBlockedUntilMillis) {
        DebugPrintln(3, "Ignoring standby wake hold because no active update task is running");
        standbySleepBlockedUntilMillis = 0;
      }
      state0();
    }
  }
  delay(20);
}
