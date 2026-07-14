#include "configMigration.h"

#include <EEPROM.h>
#include <stddef.h>
#include <string.h>

extern configData defconf;
extern int cfgStart;
extern int sizeEEPROM;

namespace {
void copyLegacyString(char *target, size_t targetSize, const char *source) {
  if (target == nullptr || targetSize == 0 || source == nullptr) {
    return;
  }
  strncpy(target, source, targetSize - 1);
  target[targetSize - 1] = '\0';
}
}  // namespace

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
  copyLegacyString(migrated.standbyAutoUpdate, sizeof(migrated.standbyAutoUpdate), legacy.standbyFirmwareUpdateCheck);
  migrated.standbyAutoUpdateIntervalHours = legacy.standbyFirmwareUpdateIntervalHours;
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
