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
#include "func_ftpclient.h"     // my lib for FTP connection (getting files for webserver)
#include "func_webclient.h"     // my lib for webclient connection (getting files for webserver)
#include <stdint.h>
#include "Configuration.h"      // Configuration

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
#include "LoRa.h"                   // LoRa Lib
#include "task.h"                   // Task for LoRa code

// Declarations
int value;                      // Value from first byte in EEPROM
int empty;                      // If EEPROM empty without configuration then set to 1 otherwise 0
configData defconf;             // Definition of default configuration data

void Task1code( void *pvParameters );  //Initiates TaskCode1

TaskHandle_t Task1;             // Declare task for LoRa code

AsyncWebServer httpServer(actconf.httpport);   // Port for HTTP server
//MDNSResponder mdns;                       // Activate DNS responder
WiFiServer server(actconf.dataport);        // Declare WiFi NMEA server port
#define MAX_CLIENTS 3 //maximal number of simultaneousy connected clients
WiFiClient clients[MAX_CLIENTS]; //Array of clients
char result[70] = "0";  //string for NMEA-assembly (dollar symbol, MWV, CS)

Ticker Timer1;                  // Declare Timer for GPS data reading
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
RTC_DATA_ATTR MdsDeviceEventSnapshot pendingMdsDeviceEventQueue[MDS_DEVICE_EVENT_QUEUE_SIZE];
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueHead = 0;
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueTail = 0;
RTC_DATA_ATTR uint8_t pendingMdsDeviceEventQueueCount = 0;

const int STATE_DELAY = 1000;
bool reboot = false;
unsigned long standbySleepBlockedUntilMillis = 0;
bool localOtaInProgress = false;
unsigned long scheduledRestartMillis = 0;
bool sendedLoraAfterSleepOneTime = false;

bool toggleDisplayStatus = false;
long LoraSendDurationSeconds = 0;
boolean runDownloadingFiles = false;
boolean runDownloadingFilesStatus = false;
size_t webFilesDownloadCompleted = 0;
size_t webFilesDownloadTotal = 0;
String webFilesDownloadCurrentName = "";
bool remoteOtaPending = false;
bool remoteOtaInProgress = false;
String remoteOtaUrl = "";
String remoteOtaVersion = "";
String remoteOtaSha256 = "";
bool remoteOtaUseMdsEndpoint = false;
bool remoteOtaRebootRequired = false;
bool wifiServicesInitialized = false;
bool mdnsInitialized = false;

const unsigned long MDS_UPLOAD_INTERVAL_MS = 300000UL;
const unsigned long WAKE_MDS_RETRY_INTERVAL_MS = 30000UL;
const unsigned long AUTO_FIRMWARE_UPDATE_RETRY_MS = 1800000UL;
const unsigned long AUTO_FIRMWARE_UPDATE_INTERVAL_MS = 21600000UL;
unsigned long lastMdsUploadMillis = 0;
unsigned long nextAutoFirmwareUpdateCheckMillis = 0;
bool pendingWakeMdsEvent = false;
int pendingWakeReasonCode = 0;
String pendingWakeReasonLabel = "unknown";
unsigned long nextWakeMdsRetryMillis = 0;
bool currentWakeMdsEventCaptured = false;

long timezone = 1; 
byte daysavetime = 1;

namespace {
struct LegacyConfigDataV16 {
  int valid = 16;
  int crypt = 1;
  char username[31] = "admin";
  char password[31] = "boatmonitor";
  char devname[21] = "LoRa Boat Monitor";
  char crights[29] = "NoWa (C) (mod by Gunni) 2023";
  char fversion[7] = "V1.13m";
  char license[12] = "GPL3";
  int debug = 3;
  int corder1 = 1;
  char cssid1[31] = "";
  char cpassword1[31] = "";
  int corder2 = 2;
  char cssid2[31] = "";
  char cpassword2[31] = "";
  int corder3 = 3;
  char cssid3[31] = "";
  char cpassword3[31] = "";
  int timeout = 10;
  char sssid[31] = "LoRaBoatMonitor";
  char spassword[31] = "LoRaBoatMonitor";
  int apchannel = 1;
  int maxconnections = 2;
  int mDNS = 1;
  char hostname[31] = "boatmonitor";
  int dataport = 6666;
  int httpport = 80;
  int serverMode = 0;
  int serspeed = 115200;
  int WebSerialDebug = 0;
  char firmwareUpdateUrl[50] = "loraboatmonitorwebserverdata.derguntmar.de";
  char autoFirmwareUpdate[8] = "No";
  int skin = 0;
  uint32_t devaddr = 0x00000000;
  uint8_t nskey[16] = {0};
  uint8_t appkey[16] = {0};
  char lorafrequency[6] = "EU868";
  int lchannel = 1;
  int spreadf = 10;
  int dynsf = 1;
  unsigned int tinterval = 1;
  uint32_t fcnt = 0;
  int relay = 0;
  int instrumentSize = 400;
  int deviceID = 0;
  int senddata = 1;
  int sendubidots = 0;
  float voffset = 6.47301;
  float a1vslope = 0.02860676;
  float a2vslope = 0;
  int vaverage = 1;
  float t1offset = 0;
  float a1t1slope = 143.1974;
  float a2t1slope = 0;
  int t1average = 1;
  float t2offset = 0;
  float a1t2slope = 143.1974;
  float a2t2slope = 0;
  int t2average = 1;
  char tempSensorType[10] = "Off";
  char tempUnit[2] = "C";
  char envSensor[20] = "Off";
  char standbyMode[4] = "Off";
  int standbySleepDuration = 15;
  char loraOperationMode[8] = "Standby";
  char WifiStandbyMode[8] = "No";
  char SendDataViaWifi[8] = "No";
  char MdsUrl[100] = "https://yourservername/ingest/receivejson.php";
  char MdsApiKey[30] = "";
  int MdsSensorIdBattery = 0;
  int MdsSensorIdTanks = 0;
  int MdsSensorIdStatus = 0;
  int MdsSensorIdGps = 0;
  int MdsSensorIdEnv = 0;
  int MdsSensorIdDewpoint = 0;
  int MdsSensorIdVedirect = 0;
  int cssStyle = 0;
  int OledDisplayRotation = 0;
  char mdsOtaUrl[100] = "https://mds-git.derguntmar.de/ota/getupdate.php";
  char mdsOtaSecret[65] = "";
  char standbyFirmwareUpdateCheck[8] = "No";
  int standbyFirmwareUpdateIntervalHours = 24;
};

void copyLegacyString(char *target, size_t targetSize, const char *source) {
  if (target == nullptr || targetSize == 0 || source == nullptr) {
    return;
  }
  strncpy(target, source, targetSize - 1);
  target[targetSize - 1] = '\0';
}

configData migrateLegacyConfigV16ToCurrent(const LegacyConfigDataV16 &legacy) {
  configData migrated = defconf;
  migrated.valid = defconf.valid;
  migrated.crypt = legacy.crypt;
  copyLegacyString(migrated.username, sizeof(migrated.username), legacy.username);
  copyLegacyString(migrated.password, sizeof(migrated.password), legacy.password);
  copyLegacyString(migrated.devname, sizeof(migrated.devname), legacy.devname);
  copyLegacyString(migrated.crights, sizeof(migrated.crights), legacy.crights);
  copyLegacyString(migrated.fversion, sizeof(migrated.fversion), legacy.fversion);
  copyLegacyString(migrated.license, sizeof(migrated.license), legacy.license);
  migrated.debug = legacy.debug;
  migrated.corder1 = legacy.corder1;
  copyLegacyString(migrated.cssid1, sizeof(migrated.cssid1), legacy.cssid1);
  copyLegacyString(migrated.cpassword1, sizeof(migrated.cpassword1), legacy.cpassword1);
  migrated.corder2 = legacy.corder2;
  copyLegacyString(migrated.cssid2, sizeof(migrated.cssid2), legacy.cssid2);
  copyLegacyString(migrated.cpassword2, sizeof(migrated.cpassword2), legacy.cpassword2);
  migrated.corder3 = legacy.corder3;
  copyLegacyString(migrated.cssid3, sizeof(migrated.cssid3), legacy.cssid3);
  copyLegacyString(migrated.cpassword3, sizeof(migrated.cpassword3), legacy.cpassword3);
  migrated.timeout = legacy.timeout;
  copyLegacyString(migrated.sssid, sizeof(migrated.sssid), legacy.sssid);
  copyLegacyString(migrated.spassword, sizeof(migrated.spassword), legacy.spassword);
  migrated.apchannel = legacy.apchannel;
  migrated.maxconnections = legacy.maxconnections;
  migrated.mDNS = legacy.mDNS;
  copyLegacyString(migrated.hostname, sizeof(migrated.hostname), legacy.hostname);
  migrated.dataport = legacy.dataport;
  migrated.httpport = legacy.httpport;
  migrated.serverMode = legacy.serverMode;
  migrated.serspeed = legacy.serspeed;
  migrated.WebSerialDebug = legacy.WebSerialDebug;
  copyLegacyString(migrated.firmwareUpdateUrl, sizeof(migrated.firmwareUpdateUrl), legacy.firmwareUpdateUrl);
  migrated.skin = legacy.skin;
  migrated.devaddr = legacy.devaddr;
  memcpy(migrated.nskey, legacy.nskey, sizeof(migrated.nskey));
  memcpy(migrated.appkey, legacy.appkey, sizeof(migrated.appkey));
  copyLegacyString(migrated.lorafrequency, sizeof(migrated.lorafrequency), legacy.lorafrequency);
  migrated.lchannel = legacy.lchannel;
  migrated.spreadf = legacy.spreadf;
  migrated.dynsf = legacy.dynsf;
  migrated.tinterval = legacy.tinterval;
  migrated.fcnt = legacy.fcnt;
  migrated.relay = legacy.relay;
  migrated.instrumentSize = legacy.instrumentSize;
  migrated.deviceID = legacy.deviceID;
  migrated.senddata = legacy.senddata;
  migrated.voffset = legacy.voffset;
  migrated.a1vslope = legacy.a1vslope;
  migrated.a2vslope = legacy.a2vslope;
  migrated.vaverage = legacy.vaverage;
  migrated.t1offset = legacy.t1offset;
  migrated.a1t1slope = legacy.a1t1slope;
  migrated.a2t1slope = legacy.a2t1slope;
  migrated.t1average = legacy.t1average;
  migrated.t2offset = legacy.t2offset;
  migrated.a1t2slope = legacy.a1t2slope;
  migrated.a2t2slope = legacy.a2t2slope;
  migrated.t2average = legacy.t2average;
  copyLegacyString(migrated.tempSensorType, sizeof(migrated.tempSensorType), legacy.tempSensorType);
  copyLegacyString(migrated.tempUnit, sizeof(migrated.tempUnit), legacy.tempUnit);
  copyLegacyString(migrated.envSensor, sizeof(migrated.envSensor), legacy.envSensor);
  copyLegacyString(migrated.standbyMode, sizeof(migrated.standbyMode), legacy.standbyMode);
  migrated.standbySleepDuration = legacy.standbySleepDuration;
  copyLegacyString(migrated.loraOperationMode, sizeof(migrated.loraOperationMode), legacy.loraOperationMode);
  copyLegacyString(migrated.WifiStandbyMode, sizeof(migrated.WifiStandbyMode), legacy.WifiStandbyMode);
  copyLegacyString(migrated.SendDataViaWifi, sizeof(migrated.SendDataViaWifi), legacy.SendDataViaWifi);
  copyLegacyString(migrated.MdsUrl, sizeof(migrated.MdsUrl), legacy.MdsUrl);
  copyLegacyString(migrated.MdsApiKey, sizeof(migrated.MdsApiKey), legacy.MdsApiKey);
  migrated.MdsSensorIdBattery = legacy.MdsSensorIdBattery;
  migrated.MdsSensorIdTanks = legacy.MdsSensorIdTanks;
  migrated.MdsSensorIdStatus = legacy.MdsSensorIdStatus;
  migrated.MdsSensorIdGps = legacy.MdsSensorIdGps;
  migrated.MdsSensorIdEnv = legacy.MdsSensorIdEnv;
  migrated.MdsSensorIdDewpoint = legacy.MdsSensorIdDewpoint;
  migrated.MdsSensorIdVedirect = legacy.MdsSensorIdVedirect;
  migrated.cssStyle = legacy.cssStyle;
  migrated.OledDisplayRotation = legacy.OledDisplayRotation;
  copyLegacyString(migrated.mdsOtaUrl, sizeof(migrated.mdsOtaUrl), legacy.mdsOtaUrl);
  copyLegacyString(migrated.mdsOtaSecret, sizeof(migrated.mdsOtaSecret), legacy.mdsOtaSecret);
  return migrated;
}

bool readLegacyConfigFromEeprom(LegacyConfigDataV16 &legacy) {
  EEPROM.begin(sizeEEPROM);
  EEPROM.get(cfgStart, legacy);
  EEPROM.end();
  return legacy.valid >= 11 && legacy.valid <= 16;
}

bool repairLegacyConfigV15(LegacyConfigDataV16 &legacy) {
  if (legacy.valid != 15) {
    return false;
  }

  constexpr size_t insertedOffset = offsetof(LegacyConfigDataV16, skin);
  constexpr size_t insertedLength = 8 + sizeof(int);
  constexpr size_t appendedOffset = offsetof(LegacyConfigDataV16, standbyFirmwareUpdateCheck);
  if (insertedOffset + insertedLength >= appendedOffset || appendedOffset >= sizeof(LegacyConfigDataV16)) {
    return false;
  }

  uint8_t rawConfig[sizeof(LegacyConfigDataV16)] = {0};
  EEPROM.begin(sizeEEPROM);
  for (size_t i = 0; i < sizeof(rawConfig); i++) {
    rawConfig[i] = EEPROM.read(cfgStart + i);
  }
  EEPROM.end();

  LegacyConfigDataV16 repaired = LegacyConfigDataV16();
  uint8_t *repairedBytes = reinterpret_cast<uint8_t*>(&repaired);
  memcpy(repairedBytes, rawConfig, insertedOffset);

  const size_t shiftedLength = appendedOffset - insertedOffset;
  memcpy(repairedBytes + insertedOffset, rawConfig + insertedOffset + insertedLength, shiftedLength);
  memcpy(repaired.standbyFirmwareUpdateCheck, rawConfig + insertedOffset, sizeof(repaired.standbyFirmwareUpdateCheck));
  memcpy(&repaired.standbyFirmwareUpdateIntervalHours,
         rawConfig + insertedOffset + sizeof(repaired.standbyFirmwareUpdateCheck),
         sizeof(repaired.standbyFirmwareUpdateIntervalHours));

  repaired.valid = 16;
  legacy = repaired;
  return true;
}
}  // namespace

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

  // A new standby event starts a new cycle. The matching wakeup slot
  // must stay empty until the device actually wakes up again.
  lastWakeupEventEpoch = 0;
  lastWakeupEventCause[0] = '\0';
}

void registerWakeupEvent(const char *eventCause) {
  lastWakeupEventEpoch = time(nullptr);
  copyEventCause(lastWakeupEventCause, sizeof(lastWakeupEventCause), eventCause);
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
  if (lastStandbyEventEpoch <= 0) {
    DebugPrintln(2, "Skipping MDS wakeup event because no standby timestamp is available");
    return false;
  }

  registerWakeupEvent(eventCause);

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
  while (now < 1704067200 && (millis() - start) < timeoutMs) {
    delay(250);
    now = time(nullptr);
  }
  return now >= 1704067200;
}

bool hasValidWakeSleepTime() {
  return time(nullptr) >= 1704067200; // 2024-01-01 00:00:00 UTC
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

  if (pendingWakeMdsEvent && !currentWakeMdsEventCaptured && hasValidWakeSleepTime()) {
    capturePendingMdsDeviceEvent(pendingWakeReasonLabel.c_str());
    pendingWakeMdsEvent = false;
  }

  if (now >= nextWakeMdsRetryMillis && loadNextQueuedMdsDeviceEvent()) {
    if (sendMdsDeviceEvent(actconf, pendingWakeReasonLabel.c_str())) {
      lastSentMdsStandbyEventEpoch = pendingMdsStandbyEventEpoch;
      lastSentMdsWakeupEventEpoch = pendingMdsWakeupEventEpoch;
      pendingMdsDeviceEventStored = false;
      currentWakeMdsEventCaptured = false;
      nextWakeMdsRetryMillis = 0;
    } else {
      nextWakeMdsRetryMillis = now + WAKE_MDS_RETRY_INTERVAL_MS;
    }
  }

  if ((lastMdsUploadMillis == 0) || (now - lastMdsUploadMillis >= MDS_UPLOAD_INTERVAL_MS)) {
    if (sendToMDS(actconf)) {
      lastMdsUploadMillis = now;
    } else {
      nextWakeMdsRetryMillis = now + WAKE_MDS_RETRY_INTERVAL_MS;
    }
  }
}

bool processPendingRemoteOta(const char *contextLabel, bool restartImmediately) {
  if (!remoteOtaPending || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  remoteOtaPending = false;
  remoteOtaInProgress = true;
  remoteOtaRebootRequired = false;

  String remoteOtaError;
  const bool remoteOtaSuccess = performRemoteOtaUpdate(remoteOtaUrl, false, remoteOtaError, remoteOtaSha256, remoteOtaUseMdsEndpoint);
  remoteOtaInProgress = false;
  remoteOtaSha256 = "";
  remoteOtaUseMdsEndpoint = false;

  if (remoteOtaSuccess && remoteOtaRebootRequired) {
    DebugPrintln(3, String(contextLabel) + " OTA update completed, reboot scheduled");
    if (restartImmediately) {
      ESP.restart();
    }
    reboot = true;
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

void prepareForStandbySleep() {
  DebugPrintln(3, "Prepare standby sleep");

  if (pendingWakeMdsEvent && !currentWakeMdsEventCaptured && hasValidWakeSleepTime()) {
    capturePendingMdsDeviceEvent(pendingWakeReasonLabel.c_str());
    pendingWakeMdsEvent = false;
  }

  if (String(actconf.SendDataViaWifi) == "Yes" && hasValidWakeSleepTime()) {
    registerStandbyEvent("Sleep standby");
  } else if (String(actconf.SendDataViaWifi) == "Yes") {
    DebugPrintln(2, "Skipping standby timestamp because system time is not synchronized");
  }

  // Re-arm wake sources right before sleep so runtime config changes
  // like standbySleepDuration apply without needing a reboot.
  esp_sleep_enable_timer_wakeup((TIME_TO_SLEEP * 60) * uS_TO_S_FACTOR);
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

      char cssid[31];
      char cpassword[31];
      
      for (int i=1; i < 4; i++) {
        if (i == 1) {
          strncpy(cssid, actconf.cssid1, sizeof(cssid) - 1);
          strncpy(cpassword, actconf.cpassword1, sizeof(cpassword) - 1);
        } else if (i == 2) {
          strncpy(cssid, actconf.cssid2, sizeof(cssid) - 1);
          strncpy(cpassword, actconf.cpassword2, sizeof(cpassword) - 1);
        } else if (i == 3) {
          strncpy(cssid, actconf.cssid3, sizeof(cssid) - 1);
          strncpy(cpassword, actconf.cpassword3, sizeof(cpassword) - 1);
        }
        cssid[sizeof(cssid) - 1] = '\0';
        cpassword[sizeof(cpassword) - 1] = '\0';

        if (cssid[0] == '\0') {
          DebugPrint(3, "Skipping empty WiFi #");
          DebugPrintln(3, i);
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
        DebugPrint(3, i);
        DebugPrint(3, " client to ");
        DebugPrintln(3, cssid);

        u8x8.clearLine(5);
        u8x8.drawString(0,5, cssid);
        u8x8.refreshDisplay();    // Only required for SSD1606/7

        // Load connection timeout from configuration (maxccount = (timeout[s] * 1000) / 200[ms])
        maxccounter = ((actconf.timeout * 1000) / 200);
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
          u8x8.drawString(0,5,"Connected IP:");
          u8x8.drawString(0,6, WiFi.localIP().toString().c_str());
          u8x8.refreshDisplay();    // Only required for SSD1606/7
          delay(100);
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
      if (waitForValidSystemTime(15000) && getLocalTime(&tmstruct, 1000)) {
        DebugPrintln(3, "\nNow is : " + String((tmstruct.tm_year)+1900) + "-" + String((tmstruct.tm_mon)+1) + "-" + String(tmstruct.tm_mday) + " " + String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));
      } else {
        DebugPrintln(1, "Time sync failed. HTTPS requests may be postponed until NTP is available.");
      }

      if (!startNetworkServices) {
        stopMdnsService();
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
    if (String(actconf.envSensor) == "VEdirect-Send") {
      if (debugPrintValues) {
        DebugPrintln(3, "VE.direct Output");
      }
      sendVEdirect();               // Send VE.direct text data
      // ":78DED000B05C4\n"
      int voltageOut = voltage * 100;
      sendBinaryValue(":78DED00", voltageOut); // Send binary data
      if(count == 0){
        sendVEdirectBinary();       // VEdirect binary data (setup and data) al 10 times
      }
    }
    count ++;
    count = count % 10;
  }

/*
  // Mirror all Ve.direct data to serial 0
  int data;
  while (Serial1.available()) {
    //Show VE.direct Daten on serial port 0
    data = Serial1.read();
    Serial.write(data);
  }
*/
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

  if (RTC_LMIC.seqnoUp != 0)    // TODO: only for OTAA mode?
  {
    LoadLMICFromRTC();
    DebugPrintln(3, "LMIC.seqnoUp restored from RTC: " + String(LMIC.seqnoUp));
  }
  else
  {
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

// S0 = Standby (Main sw off, WiFi off, Lora send every x minutes)
void state0(){
  if(machine_state0_executeOnce){
    DebugPrintln(3, " ");
    DebugPrintln(3, "state0 once");

    u8x8.clearDisplay();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"State 0   ");
    u8x8.drawString(0,1,"Batt switch off");
    u8x8.drawString(0,2,"Lora mode");
    u8x8.refreshDisplay();    // Only required for SSD1606/7

    const bool loraEnabledInStandby = (String(actconf.loraOperationMode) == "Standby" || String(actconf.loraOperationMode) == "Always");
    if (loraEnabledInStandby) {
      sendLoraQueue = true;
    }

    const bool standbyMdsUploadEnabled = (String(actconf.SendDataViaWifi) == "Yes");
    const bool standbyWifiServicesEnabled = (String(actconf.WifiStandbyMode) == "Yes");
    if (standbyWifiServicesEnabled || standbyMdsUploadEnabled) {
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

  if (String(actconf.loraOperationMode) == "Standby" || String(actconf.loraOperationMode) == "Always") {
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
      SaveLMICToRTC(TX_INTERVAL);
      delay(500); // give some time to save.
      prepareForStandbySleep();
      GoDeepSleep();
    } else if (lastPrintTime + 2000 < millis()) {
      if (toggleDisplayStatus) {
        u8x8.drawString(0,4,".");
        toggleDisplayStatus = false;
      } else {
        u8x8.drawString(0,4," ");
        toggleDisplayStatus = true;
      }

      lastPrintTime = millis();
      unsigned long difference = (lastPrintTime - loraSendDurationTime) / 1000;
      DebugPrintln(3, "difference: " + String(difference));
      long seconds = millis() / 1000;
      if (difference >= 50) {  // Abord sending, after 50 seconds
        DebugPrintln(3, "seconds >= 50");
        SaveLMICToRTC(TX_INTERVAL);
        delay(500);
        prepareForStandbySleep();
        GoDeepSleep();
      }
    }
  } else {
    // Standby must still enter deep sleep even when LoRa is disabled.
    if (String(actconf.SendDataViaWifi) == "Yes") {
      maybeSendDataViaWifi();
    }
    prepareForStandbySleep();
    GoDeepSleep();
  }

  if (String(actconf.SendDataViaWifi) == "Yes") {
    maybeSendDataViaWifi();
  }
}

// S1 = Battery On (Wifi on)
void state1(){
  if(machine_state1_executeOnce){
    DebugPrintln(3, "state1 once");
    enableWiFi();
    delay(2000);    // to be able to read the displayed infos.
    u8x8.setPowerSave(0);
    u8x8.clearDisplay();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    //u8x8.drawString(0,0,"State 1   ");
    //u8x8.drawString(0,1,"Batt switch on");
    //u8x8.drawString(0,2,"WiFI mode");
    u8x8.refreshDisplay();    // Only required for SSD1606/7

    if (actconf.relay == 1) {
      digitalWrite(relayPin, HIGH); // Relay on
    } else if (actconf.relay == 2) {
      digitalWrite(relayPin, HIGH); // Relay on
    }

    const bool loraEnabledInPowerOn = (String(actconf.loraOperationMode) == "Always" || String(actconf.loraOperationMode) == "PowerOn");
    if (loraEnabledInPowerOn) {
      Timer4.attach_ms((1000 * TX_INTERVAL), &onTimer);      // Start timer4 for sending Lora
      sendLoraQueue = true;
    } else {
      Timer4.detach();
    }
    writeDisplay();
    if (loraEnabledInPowerOn) {
      lora_init();
      do_send(&sendjob);
    }
    machine_state1_executeOnce = false;
    machine_state0_executeOnce = true;
    delay(500);
  }

  if (String(actconf.loraOperationMode) == "Always" || String(actconf.loraOperationMode) == "PowerOn") {
    lora_loop();
    static unsigned long lastPrintTime = 0;
    const bool timeCriticalJobs = os_queryTimeCriticalJobs(ms2osticksRound((TX_INTERVAL * 1000)));
    if (!timeCriticalJobs && GOTO_DEEPSLEEP == true && !(LMIC.opmode & OP_TXRXPEND)) {
      DebugPrintln(3, "Lora send done. (state1)");
      GOTO_DEEPSLEEP = false;
    }
  }
  //httpServer.handleClient();   // HTTP Server-handler for HTTP update server
  //old Voltage Offset: 6.47301

  maybeSendDataViaWifi();
  VEdirectSend();

  // Read measuring data and display on OLED all 1s
  if(millis() > starttime1 + 1000){
    starttime1 = millis();        // Read actual time

    // BME280 measuerement
    if (String(actconf.envSensor) == "BME280") {
      bme.takeForcedMeasurement(); // has no effect in normal mode
    }
    writeDisplayValues(actconf);
    if (actconf.WebSerialDebug == 1) {
      WebSerial.loop();
      //DebugPrintln(3, "Update Display.");
    }
    readValues(actconf);
  }
  VEdirectRead();
  
  // TCP-Server for NMEA0183
  WiFiClient client = server.available();// Check if a client is connected
  int i = 0;
  //client = server.available();// Check if a client is connected

  //DebugPrint(3, "client.connected(): " + String(client.connected()));
  //DebugPrintln(3, ", client.available(): " + String(client.available()));
  // Only keep this loop for active TCP clients; serial mode must return to loop()
  // so standby/alarm state changes are reevaluated continuously.
  while (client.connected() && !client.available()) {
    //DebugPrint(3, "While client connected.");
    //httpServer.handleClient();      // HTTP Server-handler for HTTP update server

    if ((i == 0) && ((int(actconf.serverMode) == 0) || (int(actconf.serverMode) == 4))) {
      DebugPrintln(3, "TCP client connected");
      DebugPrintln(3, "");
    }

    // Read measuring data and display on OLED all 1s
    if(millis() > starttime2 + 1000){
      starttime2 = millis();        // Read actual time

      // BME280 measuerement
      if (String(actconf.envSensor) == "BME280") {
        bme.takeForcedMeasurement();  // has no effect in normal mode
      }  
      //readValues();
      writeDisplay();
    }

    // Sending XDR data
    if (flag1 == true){
      i++;
      DebugPrintln(3, "");
      DebugPrint(3, "Send package:");
      DebugPrintln(3, i);

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

  if (flag2 == true) {
    //readGPSValues();
  }

  static unsigned long nextWebFilesDownloadAttempt = 0;
  if (runDownloadingFiles && WiFi.status() == WL_CONNECTED && millis() >= nextWebFilesDownloadAttempt) {
    runDownloadingFilesStatus = true;
    const bool webFilesCurrent = DownloadFilesFromWeb();
    runDownloadingFiles = !webFilesCurrent;
    nextWebFilesDownloadAttempt = webFilesCurrent ? 0 : (millis() + 30000UL);
    runDownloadingFilesStatus = false;
  }

  processPendingRemoteOta("Remote", false);

  if(reboot){
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
    case ESP_SLEEP_WAKEUP_EXT0: return "Wakeup ext0";
    case ESP_SLEEP_WAKEUP_EXT1: return "Wakeup ext1";
    case ESP_SLEEP_WAKEUP_TIMER: return "Wakeup timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Wakeup touchpad";
    case ESP_SLEEP_WAKEUP_ULP: return "Wakeup ulp";
    default: return "Wakeup unknown";
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
  currentWakeMdsEventCaptured = false;
  nextWakeMdsRetryMillis = 0;
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

  changed = sanitizeConfigRange(actconf.timeout, defconf.timeout, 3, 240) || changed;
  changed = sanitizeConfigRange(actconf.apchannel, defconf.apchannel, 1, 13) || changed;
  changed = sanitizeConfigRange(actconf.maxconnections, defconf.maxconnections, 1, 4) || changed;
  changed = sanitizeConfigRange(actconf.mDNS, defconf.mDNS, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.dataport, defconf.dataport, 1, 65535) || changed;
  changed = sanitizeConfigRange(actconf.httpport, defconf.httpport, 1, 65535) || changed;
  changed = sanitizeConfigRange(actconf.serverMode, defconf.serverMode, 0, 4) || changed;
  changed = sanitizeConfigRange(actconf.serspeed, defconf.serspeed, 300, 115200) || changed;
  changed = sanitizeConfigRange(actconf.WebSerialDebug, defconf.WebSerialDebug, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.deviceID, defconf.deviceID, 0, 9) || changed;
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
  changed = sanitizeConfigRange(actconf.standbySleepDuration, defconf.standbySleepDuration, 1, 1440) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdBattery, defconf.MdsSensorIdBattery, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdTanks, defconf.MdsSensorIdTanks, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdStatus, defconf.MdsSensorIdStatus, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdGps, defconf.MdsSensorIdGps, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdEnv, defconf.MdsSensorIdEnv, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdDewpoint, defconf.MdsSensorIdDewpoint, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.MdsSensorIdVedirect, defconf.MdsSensorIdVedirect, 0, 1) || changed;
  changed = sanitizeConfigRange(actconf.cssStyle, defconf.cssStyle, 0, 2) || changed;
  changed = sanitizeConfigRange(actconf.OledDisplayRotation, defconf.OledDisplayRotation, 0, 1) || changed;

  if (actconf.voffset == 0.0f && actconf.a1vslope == 0.0f && actconf.a2vslope == 0.0f) {
    actconf.voffset = defconf.voffset;
    actconf.a1vslope = defconf.a1vslope;
    actconf.a2vslope = defconf.a2vslope;
    changed = true;
    DebugPrintln(2, "Invalid battery calibration reset to defaults");
  }

  const char *yesNoValues[] = {"Yes", "No"};
  const char *tempSensorValues[] = {"Off", "DS18B20"};
  const char *tempUnitValues[] = {"C", "F"};
  const char *envSensorValues[] = {"Off", "BME280", "VEdirect-Read", "VEdirect-Send"};
  const char *standbyModeValues[] = {"Off", "On"};
  const char *loraModeValues[] = {"Off", "Standby", "PowerOn", "Always"};
  const char *loraFrequencyValues[] = {"EU868", "US915"};

  changed = sanitizeConfigOption(actconf.SendDataViaWifi, sizeof(actconf.SendDataViaWifi), defconf.SendDataViaWifi, yesNoValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.WifiStandbyMode, sizeof(actconf.WifiStandbyMode), defconf.WifiStandbyMode, yesNoValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.tempSensorType, sizeof(actconf.tempSensorType), defconf.tempSensorType, tempSensorValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.tempUnit, sizeof(actconf.tempUnit), defconf.tempUnit, tempUnitValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.envSensor, sizeof(actconf.envSensor), defconf.envSensor, envSensorValues, 4) || changed;
  changed = sanitizeConfigOption(actconf.standbyMode, sizeof(actconf.standbyMode), defconf.standbyMode, standbyModeValues, 2) || changed;
  changed = sanitizeConfigOption(actconf.loraOperationMode, sizeof(actconf.loraOperationMode), defconf.loraOperationMode, loraModeValues, 4) || changed;
  changed = sanitizeConfigOption(actconf.lorafrequency, sizeof(actconf.lorafrequency), defconf.lorafrequency, loraFrequencyValues, 2) || changed;

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

  if (!isPrintableConfigString(actconf.firmwareUpdateUrl, sizeof(actconf.firmwareUpdateUrl)) || strlen(actconf.firmwareUpdateUrl) == 0) {
    changed = copyConfigString(actconf.firmwareUpdateUrl, sizeof(actconf.firmwareUpdateUrl), defconf.firmwareUpdateUrl) || changed;
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
  u8x8.drawString(0,0,"Booting...");
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

  const bool hasGeneratedWebPassword = strncmp(actconf.password, "BM-", 3) == 0;
  const bool hasLegacyDefaultWebPassword = strcmp(actconf.password, "12345678") == 0 || strcmp(actconf.password, "auto-generated") == 0;
  if (hasLegacyDefaultWebPassword || hasGeneratedWebPassword || actconf.password[0] == '\0') {
    strncpy(actconf.password, defconf.password, sizeof(actconf.password) - 1);
    actconf.password[sizeof(actconf.password) - 1] = '\0';
    actconf.crypt = 1;
    saveEEPROMConfig(actconf);
    DebugPrintln(2, "Default web password set. Username: admin, password: " + String(defconf.password));
  } else if (actconf.crypt != 1) {
    actconf.crypt = 1;
    saveEEPROMConfig(actconf);
    DebugPrintln(2, "Web authentication enabled for existing configuration");
  }

  const bool hasGeneratedApPassword = strncmp(actconf.spassword, "AP-", 3) == 0;
  const bool hasLegacyDefaultApPassword = strcmp(actconf.spassword, "12345678") == 0 || strcmp(actconf.spassword, "auto-generated") == 0;
  if (hasLegacyDefaultApPassword || hasGeneratedApPassword || actconf.spassword[0] == '\0') {
    strncpy(actconf.spassword, defconf.spassword, sizeof(actconf.spassword) - 1);
    actconf.spassword[sizeof(actconf.spassword) - 1] = '\0';
    saveEEPROMConfig(actconf);
    DebugPrintln(2, "Default access point password set: " + String(defconf.spassword));
  }

  //##### Start OLED #####
  u8x8.clearDisplay();
  u8x8.drawString(0,0,"NoWa(C)OBP");
  u8x8.drawString(0,1,"mod. by Gunni");
  u8x8.drawString(10,2,actconf.fversion);
  u8x8.drawString(0,4,"Connecting to:");
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
  pinMode(alarmPin, INPUT_PULLUP);  // Alarm Pin input

  //##### Start 1Wire sensors #####
  if (String(actconf.tempSensorType) == "DS18B20") {
    sensors.begin();
  }  

  //##### Cyclic timer #####
  Timer1.attach_ms(5000, readGPSValuesFlag);     // Start timer 1 all 5s cyclic GPS data reading
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

      u8x8.drawString(0,6,"Could not find a");
      u8x8.drawString(0,7,"valid BME280!");
      u8x8.refreshDisplay();    // Only required for SSD1606/7
      delay(10000);

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

  // Create task for LoRa code
  xTaskCreatePinnedToCore(
                    Task1code,  /* Task function */
                    "Task1",    /* Name of task */
                    10000,      /* Stack size of task */
                    NULL,       /* Parameter of the task */
                    1,          /* Priority of the task */
                    &Task1,     /* Task handle to keep track of created task */
                    0);         /* Pin task to core 0 */
  //delay(500);
  delay(5);

  // sleep config...
  //Increment boot number and print it every reboot
  ++bootCount;
  //Print the wakeup reason for ESP32
  print_wakeup_reason();
  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  if (String(actconf.standbyMode) == "On") {
    esp_sleep_enable_timer_wakeup((TIME_TO_SLEEP * 60) * uS_TO_S_FACTOR);
    rtc_gpio_pullup_en(GPIO_NUM_39);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39,0);
  }

  readValues(actconf);     // initial read after boot, to get the status of alarm pin.
  DebugPrintln(3, "Standby input GPIO " + String(alarmPin) + " raw: " + String(digitalRead(alarmPin) == LOW ? "LOW" : "HIGH") + ", state: " + String(alarm1 ? "Active" : "Inactive") + " (active LOW)");

  if (String(actconf.standbyMode) == "On" && !isDeepSleepWakeup(esp_sleep_get_wakeup_cause())) {
    standbySleepBlockedUntilMillis = millis() + 180000UL;
    DebugPrintln(2, "Standby sleep blocked for 180 seconds after reboot so the web interface stays reachable.");
  }

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    DebugPrintln(1, "LittleFS Mount Failed");
    return;
  }

  if (!areWebFilesCurrent(actconf.fversion)) {
    DebugPrintln(2, "Web files version mismatch detected. Use 'Get Files from Server' in the File Manager to update them.");
  }

  sendedLoraAfterSleepOneTime = false;
}

void loop() {
  if (scheduledRestartMillis > 0 && millis() >= scheduledRestartMillis) {
    Serial.println("Scheduled restart now");
    Serial.flush();
    ESP.restart();
  }

  if (localOtaInProgress) {
    delay(20);
    return;
  }

  if ((alarm1 == true) || (String(actconf.standbyMode) == "Off")) {
    state1();
  }

  if ((alarm1 == false) && (String(actconf.standbyMode) == "On")) {
    if (millis() < standbySleepBlockedUntilMillis) {
      state1();
    } else {
      state0();
    }
  }
  delay(20);
}
