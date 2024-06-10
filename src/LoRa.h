#ifndef LoRa_h
#define LoRa_h

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui(u1_t *buf) {}
void os_getDevEui(u1_t *buf) {}
void os_getDevKey(u1_t *buf) {}

static osjob_t sendjob;

bool GOTO_DEEPSLEEP = false;

// Saves the LMIC structure during DeepSleep
RTC_DATA_ATTR lmic_t RTC_LMIC;

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

void do_send(osjob_t *j)
{
  boolean debug = true;
  boolean debugValues = false;

  // LoRa sending activ
  lora_activ = true;

  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND)
  {
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

    // int -> bytes
    byte counterLow = lowByte(LMIC.seqnoUp);
    byte counterHigh = highByte(LMIC.seqnoUp);
    // place the bytes into the payload
    mydata[0] = counterLow;
    mydata[1] = counterHigh;
    if (debug) {
      DebugPrint(3, "Packet: ");
      DebugPrintln(3, String(LMIC.seqnoUp));
    }

    // float -> int 48.234 -> 4823
    temperature16 = float2int(temperature + 50);
    // int -> bytes
    byte tempLow = lowByte(temperature16);
    byte tempHigh = highByte(temperature16);
    // place the bytes into the payload
    mydata[2] = tempLow;
    mydata[3] = tempHigh;
    if (debugValues) {
      DebugPrint(3, "Temp: ");
      DebugPrintln(3, String(temperature16));
    }

    // float -> int 48.234 -> 4823
    pressure16 = float2int(pressure / 10);
    byte pressLow = lowByte(pressure16);
    byte pressHigh = highByte(pressure16);
    // place the bytes into the payload
    mydata[4] = pressLow;
    mydata[5] = pressHigh;
    if (debugValues) {
      DebugPrint(3, "Pressure: ");
      DebugPrintln(3, String(pressure16));
    }

    // float -> int 48.234 -> 4823
    humidity16 = float2int(humidity);
    byte humLow = lowByte(humidity16);
    byte humHigh = highByte(humidity16);
    // place the bytes into the payload
    mydata[6] = humLow;
    mydata[7] = humHigh;
    if (debugValues) {
      DebugPrint(3, "Humidity: ");
      DebugPrintln(3, String(humidity16));
    }

    // float -> int 48.234 -> 4823
    dewp16 = float2int(dewp + 50);
    byte dewpLow = lowByte(dewp16);
    byte dewpHigh = highByte(dewp16);
    // place the bytes into the payload
    mydata[8] = dewpLow;
    mydata[9] = dewpHigh;
    if (debugValues) {
      DebugPrint(3, "Dewpoint: ");
      DebugPrintln(3, String(dewp16));
    }

    // float -> int 48.2345 -> 48234
    voltage16 = float3int(voltage);
    byte voltageLow = lowByte(voltage16);
    byte voltageHigh = highByte(voltage16);
    // place the bytes into the payload
    mydata[10] = voltageLow;
    mydata[11] = voltageHigh;
    if (debugValues) {
      DebugPrint(3, "Voltage: ");
      DebugPrintln(3, String(voltage16));
    }

    // float -> int 48.234 -> 4823
    temp1wire16 = float2int(temp1wire + 50);
    // int -> bytes
    byte temp2Low = lowByte(temp1wire16);
    byte temp2High = highByte(temp1wire16);
    // place the bytes into the payload
    mydata[12] = temp2Low;
    mydata[13] = temp2High;
    if (debugValues) {
      DebugPrint(3, "Temp1W: ");
      DebugPrintln(3, String(temp1wire16));
    }

    // float 48.234567 -> int 4823.4567 -> 4823 4567
    float predot = int(longitude * 100);
    float postdot = (longitude * 100) - predot;
    longitude16_1 = uint16_t(predot);
    longitude16_2 = float4int(postdot);
    byte lon1Low = lowByte(longitude16_1);
    byte lon1High = highByte(longitude16_1);
    byte lon2Low = lowByte(longitude16_2);
    byte lon2High = highByte(longitude16_2);
    // place the bytes into the payload
    mydata[14] = lon1Low;
    mydata[15] = lon1High;
    mydata[16] = lon2Low;
    mydata[17] = lon2High;
    if (debugValues) {
      DebugPrint(3, "Lon1: ");
      DebugPrintln(3, String(longitude16_1));
      DebugPrint(3, "Lon2: ");
      DebugPrintln(3, String(longitude16_2));
    }

    // float 48.234567 -> int 4823.4567 -> 4823 4567
    float predot2 = int(latitude * 100);
    float postdot2 = (latitude * 100) - predot2;
    latitude16_1 = uint16_t(predot2);
    latitude16_2 = float4int(postdot2);
    byte lat1Low = lowByte(latitude16_1);
    byte lat1High = highByte(latitude16_1);
    byte lat2Low = lowByte(latitude16_2);
    byte lat2High = highByte(latitude16_2);
    // place the bytes into the payload
    mydata[18] = lat1Low;
    mydata[19] = lat1High;
    mydata[20] = lat2Low;
    mydata[21] = lat2High;
    if (debugValues) {
      DebugPrint(3, "Lat1: ");
      DebugPrintln(3, String(latitude16_1));
      DebugPrint(3, "Lat2: ");
      DebugPrintln(3, String(latitude16_2));
    }

    // float -> int 48.234 -> 4823
    tank1_16 = float2int(tank1p);
    byte tank1Low = lowByte(tank1_16);
    byte tank1High = highByte(tank1_16);
    // place the bytes into the payload
    mydata[22] = tank1Low;
    mydata[23] = tank1High;
    if (debugValues) {
      DebugPrint(3, "Level1: ");
      DebugPrintln(3, String(tank1_16));
    }

    // float -> int 48.234 -> 4823
    tank2_16 = float2int(tank2p);
    byte tank2Low = lowByte(tank2_16);
    byte tank2High = highByte(tank2_16);
    // place the bytes into the payload
    mydata[24] = tank2Low;
    mydata[25] = tank2High;
    if (debugValues) {
      DebugPrint(3, "Level2: ");
      DebugPrintln(3, String(tank2_16));
    }

    // int -> byte
    int alarmrelay = (actconf.relay * 16) + alarm1;
    byte alarmrelayLow = lowByte(alarmrelay);
    // place the bytes into the payload
    //mydata[26] = alarmrelayLow;
    mydata[26] = alarm1;
    if (debugValues) {
      DebugPrint(3, "Alarm: ");
      DebugPrintln(3, String(alarm1));
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
    actconf.fcnt = LMIC.seqnoUp;
    saveEEPROMConfig(actconf);
    LMIC_setTxData2(1, mydata, sizeof(mydata) - 1, 0);
    DebugPrintln(3, "Packet queued");
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
      {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              DebugPrint(3, "netid: ");
              DebugPrint(3, netid, DEC);
              DebugPrint(3, "devaddr: ");
              DebugPrint(3, devaddr, HEX);
              DebugPrint(3, "artKey: ");
              for (size_t i = 0; i < sizeof(artKey); ++i)
              {
                DebugPrint(3, artKey[i], HEX);
                DebugPrint(3, "");
              }
              DebugPrintln(3, "");
              DebugPrint(3, "nwkKey: ");
              for (size_t i = 0; i < sizeof(nwkKey); ++i)
              {
                DebugPrint(3, nwkKey[i], HEX);
              }
              DebugPrintln(3, "");
          }
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
    case EV_TXCOMPLETE:
      DebugPrintln(3, "EV_TXCOMPLETE (includes waiting for RX windows)");
      if (LMIC.txrxFlags & TXRX_ACK)
        DebugPrintln(3, "Received ack");
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
              relayTimerInterrupt(); // Activate relay timer
              actconf.relay = 1;
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
                TX_INTERVAL = actconf.tinterval * 30;
                DebugPrint(3, " Downlink Massage LoRa Send interval: ");
                DebugPrint(3, String(actconf.tinterval));
                DebugPrintln(3, " x 30s");
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
      // Schedule next transmission
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      GOTO_DEEPSLEEP = true;
      break;
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
      DebugPrintln(3, "EV_TXSTART");
      break;
    case EV_TXCANCELED:
      DebugPrintln(3, "EV_TXCANCELED");
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

    // save LMIC counter to config.
    actconf.fcnt = LMIC.seqnoUp++;
    saveEEPROMConfig(actconf);

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
  DebugPrintln(3, "Load LMIC from RTC");
  LMIC = RTC_LMIC;
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
