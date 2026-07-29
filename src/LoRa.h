#ifndef LoRa_h
#define LoRa_h

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui(u1_t *buf) {}
void os_getDevEui(u1_t *buf) {}
void os_getDevKey(u1_t *buf) {}

static osjob_t sendjob;

constexpr uint8_t LORA_RX1_DELAY_SECONDS = 1;

lmic_tx_error_t lastLoraQueueResult = LMIC_ERROR_SUCCESS;
unsigned long loraPacketQueuedMillis = 0;
bool currentLoraPacketUsesWifiFallback = false;
bool loraFirstWifiFallbackPending = false;

bool isLoraPacketQueuedOrTransmitting() {
  return (LMIC.opmode & (OP_TXDATA | OP_TXRXPEND)) != 0;
}

const char *loraQueueResultName(lmic_tx_error_t result) {
  switch (result) {
    case LMIC_ERROR_SUCCESS: return "success";
    case LMIC_ERROR_TX_BUSY: return "radio busy";
    case LMIC_ERROR_TX_TOO_LARGE: return "payload too large";
    case LMIC_ERROR_TX_NOT_FEASIBLE: return "payload not feasible";
    case LMIC_ERROR_TX_FAILED: return "queue failed";
    default: return "unknown error";
  }
}

bool consumeLoraFirstWifiFallback() {
  if (!loraFirstWifiFallbackPending) {
    return false;
  }
  loraFirstWifiFallbackPending = false;
  return true;
}

bool GOTO_DEEPSLEEP = false;

// Saves the LMIC structure during DeepSleep
RTC_DATA_ATTR lmic_t RTC_LMIC;
RTC_DATA_ATTR uint32_t RTC_LMIC_CONFIG_FINGERPRINT = 0;
constexpr uint32_t LORA_FRAME_COUNTER_RESERVATION_SIZE = 32;

bool ensureLoraFrameCounterReservation() {
  if (LMIC.seqnoUp < actconf.fcnt) {
    return true;
  }
  if (LMIC.seqnoUp > UINT32_MAX - LORA_FRAME_COUNTER_RESERVATION_SIZE) {
    DebugPrintln(1, "LoRaWAN frame counter exhausted; refusing uplink");
    return false;
  }

  actconf.fcnt = LMIC.seqnoUp + LORA_FRAME_COUNTER_RESERVATION_SIZE;
  saveEEPROMConfig(actconf);
  DebugPrintln(3, "Reserved LoRaWAN frame counters through " + String(actconf.fcnt - 1));
  return true;
}

uint32_t currentLoraConfigFingerprint() {
  uint32_t hash = 2166136261UL;
  const uint8_t *devaddrBytes = reinterpret_cast<const uint8_t *>(&actconf.devaddr);
  for (size_t i = 0; i < sizeof(actconf.devaddr); ++i) hash = (hash ^ devaddrBytes[i]) * 16777619UL;
  for (uint8_t value : actconf.nskey) hash = (hash ^ value) * 16777619UL;
  for (uint8_t value : actconf.appkey) hash = (hash ^ value) * 16777619UL;
  hash = (hash ^ uint32_t(actconf.lchannel)) * 16777619UL;
  hash = (hash ^ uint32_t(actconf.spreadf)) * 16777619UL;
  hash = (hash ^ uint32_t(actconf.dynsf)) * 16777619UL;
  return hash;
}

void PrintRuntime()
{
    long seconds = millis() / 1000;
    DebugPrint(3, "Runtime: " + String(seconds) + " seconds");
}

// Set dynamically the spreading factor depends from time slot
void setSF(int tslot, int spreadingfactor, int dynamicsf){
  // If dynamic spreading factor active
  if(dynamicsf == 1){
    switch (spreadingfactor) {
    case 7:
      // SF7
      switch (tslot) {
        case 0:
          LMIC_setDrTxpow(DR_SF7,14);  // SF7
          sf = 7;
          break;
        case 1:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 2:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 3:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 4:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 5:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 6:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 7:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 8:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 9:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 10:
          LMIC_setDrTxpow(DR_SF8,14);  // SF8
          sf = 8;
          break;
        case 11:
          LMIC_setDrTxpow(DR_SF9,14);  // SF9
          sf = 9;
          break;         
        default:
          LMIC_setDrTxpow(DR_SF7,14);  // Default
          sf = 7;
          break;
      }
      break;
    case 8:
      // SF8
      switch (tslot) {
        case 0:
          LMIC_setDrTxpow(DR_SF8,14);  // SF8
          sf = 8;
          break;
        case 1:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 2:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 3:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 4:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 5:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 6:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 7:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 8:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 9:
          LMIC_setDrTxpow(DR_SF8,14);
          sf = 8;
          break;
        case 10:
          LMIC_setDrTxpow(DR_SF9,14);  // SF9
          sf = 9;
          break;
        case 11:
          LMIC_setDrTxpow(DR_SF10,14); // SF10
          sf = 10;
          break;         
        default:
          LMIC_setDrTxpow(DR_SF8,14);  // Default
          sf = 8;
          break;
      }
      break;
    case 9:
      // SF9
      switch (tslot) {
        case 0:
          LMIC_setDrTxpow(DR_SF9,14);  // SF9
          sf = 9;
          break;
        case 1:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 2:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 3:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 4:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 5:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 6:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 7:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 8:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 9:
          LMIC_setDrTxpow(DR_SF9,14);
          sf = 9;
          break;
        case 10:
          LMIC_setDrTxpow(DR_SF10,14); // SF10
          sf = 10;
          break;
        case 11:
          LMIC_setDrTxpow(DR_SF11,14); // SF11
          sf = 11;
          break;         
        default:
          LMIC_setDrTxpow(DR_SF9,14);  // Default
          sf = 9;
          break;
      }
      break;
    case 10:
      // SF10
      switch (tslot) {
        case 0:
          LMIC_setDrTxpow(DR_SF10,14);  // SF10
          sf = 10;
          break;
        case 1:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 2:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 3:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 4:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 5:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 6:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 7:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 8:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 9:
          LMIC_setDrTxpow(DR_SF10,14);
          sf = 10;
          break;
        case 10:
          LMIC_setDrTxpow(DR_SF11,14);  // SF11
          sf = 11;
          break;
        case 11:
          LMIC_setDrTxpow(DR_SF12,14);  // SF12
          sf = 12;
          break;         
        default:
          LMIC_setDrTxpow(DR_SF10,14);  // Default
          sf = 10;
          break;
      }
      break;
    default:
      // SF7
      switch (tslot) {
        case 0:
          LMIC_setDrTxpow(DR_SF7,14);  // SF7
          sf = 7;
          break;
        case 1:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 2:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 3:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 4:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 5:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 6:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 7:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 8:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 9:
          LMIC_setDrTxpow(DR_SF7,14);
          sf = 7;
          break;
        case 10:
          LMIC_setDrTxpow(DR_SF8,14);  // SF8
          sf = 8;
          break;
        case 11:
          LMIC_setDrTxpow(DR_SF9,14);  // SF9
          sf = 9;
          break;         
        default:
          LMIC_setDrTxpow(DR_SF7,14);  // Default
          sf = 7;
          break;
      }
      break;  
    }
  }
  // If dynamic spreading factor not active
  else{
    switch (spreadingfactor) {
    case 7:
      // SF7
      LMIC_setDrTxpow(DR_SF7,14);
      sf = 7;
      break;
    case 8:
      // SF8
      LMIC_setDrTxpow(DR_SF8,14);
      sf = 8;
      break;
    case 9:
      // SF9
      LMIC_setDrTxpow(DR_SF9,14);
      sf = 9;
      break;
    case 10:
      // SF10
      LMIC_setDrTxpow(DR_SF10,14);
      sf = 10;
      break;  
    default:
      // SF7
      LMIC_setDrTxpow(DR_SF7,14);
      sf = 7;
      break;
    }
  }
}

namespace {
constexpr uint8_t LORA_DYNAMIC_PORT = 1;
constexpr uint8_t LORA_STATIC_PORT = 2;
constexpr uint8_t LORA_STATIC_SCHEMA_VERSION = 1;
constexpr size_t LORA_STATIC_PAYLOAD_SIZE = 34;
RTC_DATA_ATTR uint32_t lastStaticLoraPayloadHash = 0;
uint32_t queuedStaticLoraPayloadHash = 0;
bool currentLoraPacketIsStatic = false;
bool allowStaticPacketAfterCurrentTx = false;
bool currentLoraPacketIncludedDeviceEvent = false;
time_t currentLoraPacketStandbyEpoch = 0;
time_t currentLoraPacketWakeupEpoch = 0;

void putUint16Le(uint8_t *payload, size_t offset, uint16_t value) {
  payload[offset] = lowByte(value);
  payload[offset + 1] = highByte(value);
}

void putInt16Le(uint8_t *payload, size_t offset, int16_t value) {
  putUint16Le(payload, offset, static_cast<uint16_t>(value));
}

void putUint32Le(uint8_t *payload, size_t offset, uint32_t value) {
  payload[offset] = value & 0xFF;
  payload[offset + 1] = (value >> 8) & 0xFF;
  payload[offset + 2] = (value >> 16) & 0xFF;
  payload[offset + 3] = (value >> 24) & 0xFF;
}

void putInt32Le(uint8_t *payload, size_t offset, int32_t value) {
  putUint32Le(payload, offset, static_cast<uint32_t>(value));
}

uint16_t clampUint16Value(long value) {
  if (value < 0) return 0;
  if (value > UINT16_MAX) return UINT16_MAX;
  return static_cast<uint16_t>(value);
}

int16_t clampInt16Value(long value) {
  if (value < INT16_MIN) return INT16_MIN;
  if (value > INT16_MAX) return INT16_MAX;
  return static_cast<int16_t>(value);
}

uint8_t encodeLoraOperationMode(const char *mode) {
  if (strcmp(mode, "Standby") == 0) return 1;
  if (strcmp(mode, "PowerOn") == 0) return 2;
  if (strcmp(mode, "Always") == 0) return 3;
  return 0;
}

uint8_t encodeEnvironmentSensor(const char *sensor) {
  if (strcmp(sensor, "BME280") == 0) return 1;
  if (strcmp(sensor, "VEdirect-Read") == 0) return 2;
  if (strcmp(sensor, "VEdirect-Send") == 0) return 3;
  return 0;
}

uint32_t hashLoraPayload(const uint8_t *payload, size_t length) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; ++i) {
    hash ^= payload[i];
    hash *= 16777619UL;
  }
  return hash;
}

uint8_t encodeStandbyCause(const char *cause) {
  if (cause != nullptr && strcmp(cause, "Sleep standby") == 0) return 1;
  return cause != nullptr && cause[0] != '\0' ? 15 : 0;
}

uint8_t encodeWakeupCause(const char *cause) {
  if (cause == nullptr || cause[0] == '\0') return 0;
  if (strcmp(cause, "Wakeup EXT0") == 0) return 1;
  if (strcmp(cause, "Wakeup EXT1") == 0) return 2;
  if (strcmp(cause, "Wakeup Timer") == 0) return 3;
  if (strcmp(cause, "Wakeup Touch") == 0) return 4;
  if (strcmp(cause, "Wakeup ULP") == 0) return 5;
  if (strcmp(cause, "Wakeup Other") == 0) return 6;
  return 15;
}

void buildDynamicLoraPayload() {
  memset(mydata, 0, sizeof(mydata));
  mydata[0] = 3;
  putUint16Le(mydata, 1, static_cast<uint16_t>(LMIC.seqnoUp & 0xFFFF));
  putInt16Le(mydata, 3, clampInt16Value(lroundf(temperature * 10.0f)));
  putUint16Le(mydata, 5, clampUint16Value(lroundf(pressure * 10.0f)));
  mydata[7] = static_cast<uint8_t>(constrain(lroundf(humidity), 0L, 100L));
  putInt16Le(mydata, 8, clampInt16Value(lroundf(dewp * 10.0f)));
  putUint16Le(mydata, 10, clampUint16Value(lroundf(voltage * 1000.0f)));
  putInt16Le(mydata, 12, clampInt16Value(lroundf(temp1wire * 10.0f)));
  putInt32Le(mydata, 14, static_cast<int32_t>(lroundf(longitude * 1000000.0f)));
  putInt32Le(mydata, 18, static_cast<int32_t>(lroundf(latitude * 1000000.0f)));
  mydata[22] = static_cast<uint8_t>(constrain(lroundf(tank1p), 0L, 100L));
  mydata[23] = static_cast<uint8_t>(constrain(lroundf(tank2p), 0L, 100L));

  uint8_t status = ((actconf.relay & 0x03) << 4) | (mainPowerOn & 0x01);
  if (strcmp(actconf.envSensor, "BME280") == 0) status |= 0x04;
  if (strcmp(actconf.envSensor, "VEdirect-Read") == 0) status |= 0x08;
  if (gpsStatus == "A") status |= 0x40;

  time_t standbyEpoch = 0;
  time_t wakeupEpoch = 0;
  char standbyCause[24] = "";
  char wakeupCause[24] = "";
  currentLoraPacketIncludedDeviceEvent = getPendingLoraDeviceEvent(
    standbyEpoch, wakeupEpoch,
    standbyCause, sizeof(standbyCause),
    wakeupCause, sizeof(wakeupCause)
  );
  currentLoraPacketStandbyEpoch = currentLoraPacketIncludedDeviceEvent ? standbyEpoch : 0;
  currentLoraPacketWakeupEpoch = currentLoraPacketIncludedDeviceEvent ? wakeupEpoch : 0;
  if (currentLoraPacketIncludedDeviceEvent) status |= 0x80;
  mydata[24] = status;

  mydata[25] = static_cast<uint8_t>(constrain(lroundf(capacity), 0L, 100L));
  putUint16Le(mydata, 26, tank1adc);
  putUint16Le(mydata, 28, tank2adc);
  putUint16Le(mydata, 30, clampUint16Value(lroundf(gpsspeed * 100.0f)));
  putUint16Le(mydata, 32, clampUint16Value(lroundf(course * 100.0f)));
  putInt16Le(mydata, 34, clampInt16Value(lroundf(altitude * 10.0f)));
  putUint16Le(mydata, 36, clampUint16Value(lroundf(vedirectVoltage * 100.0f)));
  putInt16Le(mydata, 38, clampInt16Value(lroundf(vedirectCurrent * 100.0f)));
  putInt16Le(mydata, 40, clampInt16Value(lroundf(vedirectTemp * 100.0f)));
  putUint32Le(mydata, 42, currentLoraPacketIncludedDeviceEvent ? static_cast<uint32_t>(standbyEpoch) : 0);
  putUint32Le(mydata, 46, currentLoraPacketIncludedDeviceEvent ? static_cast<uint32_t>(wakeupEpoch) : 0);
  mydata[50] = (encodeWakeupCause(wakeupCause) << 4) | encodeStandbyCause(standbyCause);

  if (currentLoraPacketIncludedDeviceEvent) {
    DebugPrintln(3, "FPort 1 includes WakeupLog timestamps and causes");
  }
}

uint32_t buildStaticLoraPayload(uint8_t (&payload)[LORA_STATIC_PAYLOAD_SIZE]) {
  memset(payload, 0, sizeof(payload));
  payload[0] = LORA_STATIC_SCHEMA_VERSION;
  payload[1] = (strcmp(actconf.standbyMode, "On") == 0 ? 0x01 : 0) |
               (strcmp(actconf.WifiStandbyMode, "Yes") == 0 ? 0x02 : 0) |
               (strcmp(actconf.SendDataViaWifi, "Yes") == 0 ? 0x04 : 0) |
               (actconf.dynsf ? 0x08 : 0) |
               (actconf.mDNS ? 0x10 : 0) |
               (actconf.crypt ? 0x20 : 0) |
               (strcmp(FIRMWARE_RELEASE_CHANNEL, "stable") == 0 ? 0x40 : 0);
  payload[2] = static_cast<uint8_t>(constrain(actconf.valid, 0, 255));
  payload[3] = static_cast<uint8_t>(constrain(actconf.tinterval, 1U, 255U));
  putUint16Le(payload, 4, clampUint16Value(actconf.standbySleepDuration));
  payload[6] = static_cast<uint8_t>(constrain(actconf.standbyAutoUpdateIntervalHours, 1, 255));
  payload[7] = strcmp(actconf.transmitPriority, "WifiFirst") == 0 ? 1 : 0;
  payload[8] = encodeLoraOperationMode(actconf.loraOperationMode);
  payload[9] = static_cast<uint8_t>(constrain(actconf.spreadf, 7, 12));
  payload[10] = static_cast<uint8_t>(constrain(actconf.lchannel, 0, 255));
  payload[11] = static_cast<uint8_t>(constrain(actconf.serverMode, 0, 255));
  payload[12] = strcmp(actconf.tempSensorType, "DS18B20") == 0 ? 1 : 0;
  payload[13] = encodeEnvironmentSensor(actconf.envSensor);
  memcpy(payload + 14, actconf.fversion, min(sizeof(actconf.fversion), size_t(8)));
  payload[22] = static_cast<uint8_t>(constrain(actconf.deviceID, 0, 255));
  payload[23] = static_cast<uint8_t>(constrain(actconf.relay, 0, 2));
  for (uint8_t i = 0; i < 6; ++i) payload[24 + i] = (macAddress >> (8 * i)) & 0xFF;

  const uint32_t hash = hashLoraPayload(payload, 30);
  payload[30] = hash & 0xFF;
  payload[31] = (hash >> 8) & 0xFF;
  payload[32] = (hash >> 16) & 0xFF;
  payload[33] = (hash >> 24) & 0xFF;
  return hash;
}

bool queueStaticLoraPayloadIfChanged() {
  uint8_t payload[LORA_STATIC_PAYLOAD_SIZE];
  const uint32_t payloadHash = buildStaticLoraPayload(payload);
  if (payloadHash == lastStaticLoraPayloadHash) return false;
  if (!ensureLoraFrameCounterReservation()) return false;

  lastLoraQueueResult = LMIC_setTxData2(LORA_STATIC_PORT, payload, sizeof(payload), 0);
  if (lastLoraQueueResult != LMIC_ERROR_SUCCESS) {
    DebugPrintln(1, "Static LoRa device packet rejected: " + String(loraQueueResultName(lastLoraQueueResult)));
    return false;
  }

  queuedStaticLoraPayloadHash = payloadHash;
  currentLoraPacketIsStatic = true;
  loraPacketQueuedMillis = millis();
  DebugPrintln(3, "Static LoRa device packet queued on FPort 2");
  return true;
}
}

void do_send(osjob_t *j)
{
  boolean debug = true;
  boolean debugValues = false;

  // LoRa sending activ
  lora_activ = true;

  // A successful Wi-Fi/MDS delivery replaces this scheduled LoRa uplink when
  // the configured strategy is Wi-Fi first. Manual test packets are never
  // suppressed.
  if (!isManualLoraSendBusy() && consumeSuccessfulWifiDeliveryForLoraFallback()) {
    DebugPrintln(3, "LoRa uplink skipped because WiFi/MDS delivery succeeded");
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
    lora_activ = false;
    return;
  }

  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND)
  {
    lastLoraQueueResult = LMIC_ERROR_TX_BUSY;
    DebugPrintln(3, "OP_TXRXPEND, curently not sending");
  } else {
    // Prepare upstream data transmission at the next possible time.

    // Slot calculation
    slot = slotcounter % 12;
    if (debug) {
      DebugPrint(3, "Slot = ");
      DebugPrint(3, String(slot));
      DebugPrintln(3, "");
    }

    // Set different spreading factor for time slots
    //setSF(slot, actconf.spreadf, actconf.dynsf);

    // Increment slot counter
    slotcounter++;

    // Read sensor values (BME280, DS18B20, ...)
    readValues(actconf);

    buildDynamicLoraPayload();
    if (debug) {
      DebugPrint(3, "Packet: ");
      DebugPrintln(3, String(LMIC.seqnoUp));
    }

    // Relay
    if (debugValues) {
      DebugPrint(3, "Relay: ");
      DebugPrintln(3, String(actconf.relay));
    }

    String payload = "";
    //        sprintf(payload,"%x",mydata);
    if (debugValues) {
      DebugPrint(3, "Payload: " + String(payload));
    }

    //        flashLED(100);  // Flash white LED on LoRa board
    loraSendDurationTime = millis();
    if (!ensureLoraFrameCounterReservation()) {
      lora_activ = false;
      failManualLoraSend("LoRaWAN frame counter exhausted.");
      return;
    }
    const bool manualUplink = isManualLoraSendBusy();
    currentLoraPacketIsStatic = false;
    allowStaticPacketAfterCurrentTx = !manualUplink;
    currentLoraPacketUsesWifiFallback = !manualUplink &&
      strcmp(actconf.transmitPriority, "LoRaFirst") == 0 &&
      strcmp(actconf.SendDataViaWifi, "Yes") == 0;
    // WakeupLog entries must survive radio transmission without a network ACK.
    const bool confirmedUplink = manualUplink || currentLoraPacketUsesWifiFallback || currentLoraPacketIncludedDeviceEvent;
    lastLoraQueueResult = LMIC_setTxData2(
      LORA_DYNAMIC_PORT,
      mydata,
      sizeof(mydata),
      confirmedUplink ? 1 : 0
    );
    if (lastLoraQueueResult == LMIC_ERROR_SUCCESS) {
      loraPacketQueuedMillis = millis();
      DebugPrintln(3, confirmedUplink ? "Confirmed packet queued" : "Packet queued");
    } else {
      DebugPrintln(1, "LoRa packet rejected: " + String(loraQueueResultName(lastLoraQueueResult)));
      if (currentLoraPacketUsesWifiFallback) {
        loraFirstWifiFallbackPending = true;
      }
      currentLoraPacketUsesWifiFallback = false;
      currentLoraPacketIncludedDeviceEvent = false;
      currentLoraPacketStandbyEpoch = 0;
      currentLoraPacketWakeupEpoch = 0;
    }
  }
  // Next TX is scheduled after TX_COMPLETE event.

  // LoRa sending end
  lora_activ = false;
}

void onEvent(ev_t ev) {
  // LoRa sending activ
  loraEvent_activ = true;

  DebugPrint(3, String(os_getTime()) + String(": "));
  printLocalTime();

  switch (ev) {
    case EV_SCAN_TIMEOUT:
      DebugPrintln(3, "EV_SCAN_TIMEOUT");
      break;
    case EV_BEACON_FOUND:
      DebugPrintln(3, "EV_BEACON_FOUND");
      break;
    case EV_BEACON_MISSED:
      DebugPrintln(3, "EV_BEACON_MISSED");
      break;
    case EV_BEACON_TRACKED:
      DebugPrintln(3, "EV_BEACON_TRACKED");
      break;
    case EV_JOINING:
      DebugPrintln(3, "EV_JOINING");
      break;
    case EV_JOINED:
      DebugPrintln(3, "EV_JOINED");
          // Disable link check validation (automatically enabled
          // during join, but because slow data rates change max TX
          // size, we don't use it in this example.
          LMIC_setLinkCheckMode(0);
      break;
    case EV_RFU1:
      DebugPrintln(3, "EV_RFU1");
      break;
    case EV_JOIN_FAILED:
      DebugPrintln(3, "EV_JOIN_FAILED");
      break;
    case EV_REJOIN_FAILED:
      DebugPrintln(3, "EV_REJOIN_FAILED");
      break;
    case EV_TXCOMPLETE: {
      loraPacketQueuedMillis = 0;
      DebugPrintln(3, "EV_TXCOMPLETE (includes waiting for RX windows)");
      const bool acknowledged = (LMIC.txrxFlags & TXRX_ACK) != 0;
      if (acknowledged) {
        DebugPrintln(3, "Received ack");
      }
      const bool standaloneManualUplink = completeManualLoraSend(
        acknowledged,
        LMIC.dataLen
      );
      if (LMIC.dataLen) {
        // data received in rx slot after tx
        DebugPrint(3, "Received ");
        DebugPrint(3, String(LMIC.dataLen));
        DebugPrintln(3, " bytes of payload");
        for (int i = 0; i < LMIC.dataLen; i++)
          {
            DebugPrint(3, "0x");
            if (LMIC.frame[LMIC.dataBeg + i] < 0x10)
            {
              DebugPrint(3, "0");
            }
            DebugPrint(3, String(LMIC.frame[LMIC.dataBeg + i], HEX));

            //*********************************************
            // Downlink Byte 0
            // ################
            // Downlink message for relais
            if (i == 0)
            {
              rpayload[0] = LMIC.frame[LMIC.dataBeg + i];
            }

            // Downlink Byte 1
            // ################
            // Downlink message spreading factor
            if (i == 1)
            {
              rpayload[1] = LMIC.frame[LMIC.dataBeg + i];
            }

            // Downlink Byte 2
            // ################
            // Downlink message LoRa send interval
            if (i == 2)
            {
              rpayload[2] = LMIC.frame[LMIC.dataBeg + i];
            }

            // Downlink Byte 3
            // ################
            // Downlink message CRC
            if (i == 3)
            {
              rpayload[3] = LMIC.frame[LMIC.dataBeg + i];
            }
            //*********************************************
          }

        // Check telegram length
        DebugPrintln(3, "");
        if (LMIC.dataLen == 4)
          {
            DebugPrintln(1, "Downlink Message: length ok");
            // Check telegram CRC
            if (rpayload[0] + rpayload[1] + rpayload[2] == rpayload[3])
            {
              DebugPrint(1, "CRC: ");
              DebugPrintln(1, String(rpayload[3]));
              DebugPrintln(1, "CRC: ok");

              // Byte 0, set relay time (Relay on)
              relaytimer = rpayload[0];
              actconf.relay = (relaytimer > 0) ? 1 : 0;
              digitalWrite(relayPin, (relaytimer > 0) ? HIGH : LOW);
              DebugPrint(3, " Downlink Massage Relay: ");
              DebugPrint(3, String(relaytimer));
              DebugPrintln(3, " x 5min");

              // Byte 1, set spreeding factor
              if (rpayload[1] >= 7 && rpayload[1] <= 10)
              {
                actconf.spreadf = rpayload[1];
                DebugPrint(3, " Downlink Massage SF: ");
                DebugPrintln(3, String(actconf.spreadf));
              }
              else
              {
                DebugPrintln(1, "Downlink Massage SF: Error");
              }

              // Byte 2, set send interval
              if (rpayload[2] > 0)
              {
                actconf.tinterval = rpayload[2];
                TX_INTERVAL = actconf.tinterval * 60;
                DebugPrint(3, " Downlink Massage LoRa Send interval: ");
                DebugPrint(3, String(actconf.tinterval));
                DebugPrintln(3, " x 60s");
              }
              else
              {
                DebugPrintln(1, "Downlink Massage Send Interval: Error");
              }

              // Save settings in EEPROM
              DebugPrintln(3, "New downlink settings saved");
              saveEEPROMConfig(actconf);
              DebugPrintln(3, "");
            }
            else
            {
              DebugPrint(1, "CRC: ");
              DebugPrintln(1, String(rpayload[3]));
              DebugPrintln(1, "CRC: Error");
            }
          }
        else
        {
          DebugPrintln(1, "Downlink Message: unknown");
        }
      }
      const bool dynamicDeliveryAccepted = currentLoraPacketIncludedDeviceEvent
        ? acknowledged
        : (!currentLoraPacketUsesWifiFallback || acknowledged);
      if (!currentLoraPacketIsStatic && currentLoraPacketIncludedDeviceEvent && dynamicDeliveryAccepted) {
        acknowledgePendingLoraDeviceEvent();
        acknowledgeMdsDeviceEventDeliveredByLora(currentLoraPacketStandbyEpoch,
                                                 currentLoraPacketWakeupEpoch);
        currentLoraPacketIncludedDeviceEvent = false;
        currentLoraPacketStandbyEpoch = 0;
        currentLoraPacketWakeupEpoch = 0;
        DebugPrintln(3, "LoRa WakeupLog event acknowledged after TX complete");
      }
      if (currentLoraPacketIsStatic) {
        lastStaticLoraPayloadHash = queuedStaticLoraPayloadHash;
        currentLoraPacketIsStatic = false;
      } else {
        if (currentLoraPacketUsesWifiFallback) {
          loraFirstWifiFallbackPending = !acknowledged;
          DebugPrintln(3, acknowledged
            ? "LoRa-first delivery acknowledged; WiFi fallback not needed"
            : "LoRa-first delivery not acknowledged; WiFi fallback requested");
        }
        currentLoraPacketUsesWifiFallback = false;
        if (dynamicDeliveryAccepted && allowStaticPacketAfterCurrentTx && queueStaticLoraPayloadIfChanged()) {
          GOTO_DEEPSLEEP = false;
          break;
        }
      }
      // A standalone manual test must not silently enable periodic LoRa sends.
      if (!standaloneManualUplink) {
        os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      } else {
        os_clearCallback(&sendjob);
      }
      GOTO_DEEPSLEEP = !standaloneManualUplink;
      break;
    }
    case EV_LOST_TSYNC:
      DebugPrintln(3, "EV_LOST_TSYNC");
      break;
    case EV_RESET:
      DebugPrintln(3, "EV_RESET");
      break;
    case EV_RXCOMPLETE:
      // data received in ping slot
      DebugPrintln(3, "EV_RXCOMPLETE");
      break;
    case EV_LINK_DEAD:
      DebugPrintln(3, "EV_LINK_DEAD");
      break;
    case EV_LINK_ALIVE:
      DebugPrintln(3, "EV_LINK_ALIVE");
      break;
    case EV_TXSTART:
      if (loraPacketQueuedMillis == 0) {
        loraPacketQueuedMillis = millis();
      }
      DebugPrintln(3, "EV_TXSTART");
      noteManualLoraTxStarted();
      break;
    case EV_TXCANCELED:
      loraPacketQueuedMillis = 0;
      if (currentLoraPacketUsesWifiFallback) {
        loraFirstWifiFallbackPending = true;
        currentLoraPacketUsesWifiFallback = false;
      }
      DebugPrintln(3, "EV_TXCANCELED");
      failManualLoraSend("LMIC canceled the manual LoRa transmission.");
      break;
    case EV_RXSTART:
      /* do not print anything -- it wrecks timing */
      break;
    case EV_JOIN_TXCOMPLETE:
      DebugPrintln(3, "EV_JOIN_TXCOMPLETE: no JoinAccept");
      break;
    default:
      DebugPrintln(3, "Unknown event: ");
      DebugPrintln(3, String(ev));
      break;
  }
  // LoRa sending end
  loraEvent_activ = false;
}

// opmode def
// https://github.com/mcci-catena/arduino-lmic/blob/89c28c5888338f8fc851851bb64968f2a493462f/src/lmic/lmic.h#L233
void LoraWANPrintLMICOpmode(void)
{
    DebugPrint(3, F("LMIC.opmode: "));
    if (LMIC.opmode & OP_NONE)
    {
        DebugPrint(3, F("OP_NONE "));
    }
    if (LMIC.opmode & OP_SCAN)
    {
        DebugPrint(3, F("OP_SCAN "));
    }
    if (LMIC.opmode & OP_TRACK)
    {
        DebugPrint(3, F("OP_TRACK "));
    }
    if (LMIC.opmode & OP_JOINING)
    {
        DebugPrint(3, F("OP_JOINING "));
    }
    if (LMIC.opmode & OP_TXDATA)
    {
        DebugPrint(3, F("OP_TXDATA "));
    }
    if (LMIC.opmode & OP_POLL)
    {
        DebugPrint(3, F("OP_POLL "));
    }
    if (LMIC.opmode & OP_REJOIN)
    {
        DebugPrint(3, F("OP_REJOIN "));
    }
    if (LMIC.opmode & OP_SHUTDOWN)
    {
        DebugPrint(3, F("OP_SHUTDOWN "));
    }
    if (LMIC.opmode & OP_TXRXPEND)
    {
        DebugPrint(3, F("OP_TXRXPEND "));
    }
    if (LMIC.opmode & OP_RNDTX)
    {
        DebugPrint(3, F("OP_PINGINI "));
    }
    if (LMIC.opmode & OP_PINGINI)
    {
        DebugPrint(3, F("OP_PINGINI "));
    }
    if (LMIC.opmode & OP_PINGABLE)
    {
        DebugPrint(3, F("OP_PINGABLE "));
    }
    if (LMIC.opmode & OP_NEXTCHNL)
    {
        DebugPrint(3, F("OP_NEXTCHNL "));
    }
    if (LMIC.opmode & OP_LINKDEAD)
    {
        DebugPrint(3, F("OP_LINKDEAD "));
    }
    if (LMIC.opmode & OP_LINKDEAD)
    {
        DebugPrint(3, F("OP_LINKDEAD "));
    }
    if (LMIC.opmode & OP_TESTMODE)
    {
        DebugPrint(3, F("OP_TESTMODE "));
    }
    if (LMIC.opmode & OP_UNJOIN)
    {
        DebugPrint(3, F("OP_UNJOIN "));
    }
    DebugPrintln(3, "");
}

void LoraWANDebug(lmic_t lmic_check)
{
    DebugPrintln(3, "");
    LoraWANPrintLMICOpmode();
    String MyMessage = "";

    MyMessage += (3, "LMIC.seqnoUp = " + String(lmic_check.seqnoUp) + "LMIC.globalDutyRate = " + String(lmic_check.globalDutyRate) + " osTicks, " + String(osticks2ms(lmic_check.globalDutyRate) / 1000) + " sec\n");

    MyMessage += (3, "LMIC.globalDutyAvail = " + String(lmic_check.globalDutyAvail) + F(" osTicks, ") + String(osticks2ms(lmic_check.globalDutyAvail) / 1000) + F(" sec\n"));

    MyMessage += (3, "os_getTime = " + String(os_getTime()) + F(" osTicks, ") + String(osticks2ms(os_getTime()) / 1000) + F(" sec\n"));

    MyMessage += (3, "LMIC.txend = " + String(lmic_check.txend) + "LMIC.txChnl = " + String(lmic_check.txChnl) + "\n");

    MyMessage += (3, F("Band \tavail \t\tavail_sec\tlastchnl \ttxcap\n"));
    for (u1_t bi = 0; bi < MAX_BANDS; bi++)
    {
        MyMessage += (3, String(bi) + "\t" + String(lmic_check.bands[bi].avail) + "\t\t" + String(osticks2ms(lmic_check.bands[bi].avail) / 1000) + "\t\t" + String(lmic_check.bands[bi].lastchnl) + "\t\t" + String(lmic_check.bands[bi].txcap) + "\n");
    }
    DebugPrintln(3, MyMessage);
    DebugPrintln(3, "");
}

void SaveLMICToRTC(int deepsleep_sec)
{
    DebugPrintln(3, "Save LMIC to RTC");
    RTC_LMIC = LMIC;
    RTC_LMIC_CONFIG_FINGERPRINT = currentLoraConfigFingerprint();

    // The persistent high-water mark is reserved before transmission. RTC
    // keeps the exact counter across deep sleep without another flash write.

    // ESP32 can't track millis during DeepSleep and no option to advanced millis after DeepSleep.
    // Therefore reset DutyCyles

    unsigned long now = millis();

    // EU Like Bands
#if defined(CFG_LMIC_EU_like)
    DebugPrintln(3, "Reset CFG_LMIC_EU_like band avail");
    for (int i = 0; i < MAX_BANDS; i++)
    {
        ostime_t correctedAvail = RTC_LMIC.bands[i].avail - ((now / 1000.0 + deepsleep_sec) * OSTICKS_PER_SEC);
        if (correctedAvail < 0)
        {
            correctedAvail = 0;
        }
        RTC_LMIC.bands[i].avail = correctedAvail;
    }

    RTC_LMIC.globalDutyAvail = RTC_LMIC.globalDutyAvail - ((now / 1000.0 + deepsleep_sec) * OSTICKS_PER_SEC);
    if (RTC_LMIC.globalDutyAvail < 0)
    {
        RTC_LMIC.globalDutyAvail = 0;
    }
#else
    DebugPrintln(3, "No DutyCycle recalculation function!");
#endif
}

void LoadLMICFromRTC()
{
  DebugPrintln(3, "Load persistent LMIC counters from RTC");

  // The LMIC scheduler is rebuilt after deep sleep. Restoring its transient
  // OP_TXDATA/OP_TXRXPEND state can leave a packet pending without a radio job.
  LMIC.seqnoUp = RTC_LMIC.seqnoUp;
  LMIC.seqnoDn = RTC_LMIC.seqnoDn;

#if defined(CFG_LMIC_EU_like)
  for (int i = 0; i < MAX_BANDS; i++) {
    LMIC.bands[i].avail = RTC_LMIC.bands[i].avail;
  }
  LMIC.globalDutyAvail = RTC_LMIC.globalDutyAvail;
#endif

  loraPacketQueuedMillis = 0;
  lastLoraQueueResult = LMIC_ERROR_SUCCESS;
}

uint8_t getLMICtxChnl() {
  return LMIC.txChnl;
}

uint32_t getLMICseqnoUp() {
  return LMIC.seqnoUp;
}

void GoDeepSleep()
{
  DebugPrintln(3, "Go DeepSleep");
  PrintRuntime();
  Serial.flush();
  Serial2.flush();
  esp_deep_sleep_start();
}

// Enable the used LoRa channels
void setChannel(int channel){
  switch (channel) {
  case 0:
    // Single channel 0
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 1:
    // Single channel 1
    LMIC_disableChannel (0);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 2:
    // Single channel 2
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 3:
    // Single channel 3
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 4:
    // Single channel 4
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 5:
    // Single channel 5
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  case 6:
    // Single channel 6
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (7);
    break;
  case 7:
    // Single channel 7
    LMIC_disableChannel (0);
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    break;
  case 8:
    // Multi channel 0...7
    break;
  case 9:
    // Multi channel 0...2
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;        
  default:
    // Channel 0
    LMIC_disableChannel (1);
    LMIC_disableChannel (2);
    LMIC_disableChannel (3);
    LMIC_disableChannel (4);
    LMIC_disableChannel (5);
    LMIC_disableChannel (6);
    LMIC_disableChannel (7);
    break;
  }
}

#endif
