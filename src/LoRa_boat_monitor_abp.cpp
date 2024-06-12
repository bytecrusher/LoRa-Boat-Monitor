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
#include "FS.h"                 // FS lib
#include <LittleFS.h>           // Lib for LittleFS filesystem
#include <time.h>               // Time lib
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
configData oldconf;             // Configuration stucture for old config data in EEPROM
configData newconf;             // Configuration stucture for new config data in EEPROM

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

int state1counter = 0;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  actconf.standbySleepDuration       /* Time ESP32 will go to sleep */
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR uint loraCount = 0;

const int STATE_DELAY = 1000;
bool reboot = false;
bool sendedLoraAfterSleepOneTime = false;

bool toggleDisplayStatus = false;
long LoraSendDurationSeconds = 0;
boolean runDownloadingFiles = false;
boolean runDownloadingFilesStatus = false;

long timezone = 1; 
byte daysavetime = 1;

File root;

void IRAM_ATTR onTimer(){
  DebugPrintln(3, "onTimer reached");
  sendLoraQueue = true;
}

void enableWiFi(){
  //adc_power_on();
  WiFi.disconnect(false);  // Reconnect the network
  WiFi.hostname(hname);   // Provide the hostname
  //WiFi.setHostname (hname.c_str()); /*ESP32 hostname set*/

  //*****************************************************************************************
      // Starting access point for update server
      DebugPrint(3, "Access point started with SSID: ");
      DebugPrintln(3, actconf.sssid);
      DebugPrint(3, "Access point channel: ");
      DebugPrintln(3, WiFi.channel());
    //  DebugPrintln(3, actconf.apchannel);
      DebugPrint(3, "Max AP connections: ");
      DebugPrintln(3, actconf.maxconnections);
      //WiFi.mode(WIFI_AP_STA);
      WiFi.mode(WIFI_MODE_APSTA);
      //WiFi.softAP(actconf.sssid, actconf.spassword, actconf.apchannel, false, actconf.maxconnections);
      WiFi.softAP(actconf.sssid, actconf.spassword);
      hname = String(actconf.hostname) + "-" + String(actconf.deviceID);
      
      DebugPrint(3, "Host name: ");
      DebugPrintln(3, hname);

      char cssid[31];
      char cpassword[31];
      
      for (int i=1; i < 4; i++) {
        if (i == 1) {
          strncpy(cssid, actconf.cssid1, 31);
          strncpy(cpassword, actconf.cpassword1, 31);
        } else if (i == 2) {
          strncpy(cssid, actconf.cssid2, 31);
          strncpy(cpassword, actconf.cpassword2, 31);
        } else if (i == 3) {
          strncpy(cssid, actconf.cssid3, 31);
          strncpy(cpassword, actconf.cpassword3, 31);
        }

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

        // Wait until is connected otherwise abort connection after x connection trys
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
          WiFi.disconnect(true);                // Abort connection
          DebugPrintln(3, "Connection aborted");
          DebugPrintln(3, "");
          //u8x8.drawString(0,3,"Conection aborted");
          //u8x8.refreshDisplay();    // Only required for SSD1606/7
        }
      }

      if(actconf.mDNS == 1){
        MDNS.end();
        MDNS.begin(hname.c_str());                              // Start mDNS service
        MDNS.addService("http", "tcp", actconf.httpport);       // HTTP service
        MDNS.addService("nmea-0183", "tcp", actconf.dataport);  // NMEA0183 dada service for AVnav
        DebugPrintln(3, "mDNS service: activ");
        DebugPrintln(3, "mDNS name: " + String(hname) + ".local");
        //DebugPrint(3, hname);
        //DebugPrintln(3, ".local");
      }

      DebugPrintln(3, "Contacting Time Server");
	    configTime(3600*timezone, daysavetime*3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
	    struct tm tmstruct ;
      tmstruct.tm_year = 0;
      getLocalTime(&tmstruct, 5000);
      DebugPrintln(3, "\nNow is : " + String((tmstruct.tm_year)+1900) + "-" + String(( tmstruct.tm_mon)+1) + "-" + String(tmstruct.tm_mday) + " " + String(tmstruct.tm_hour) + ":" + String(tmstruct.tm_min) + ":" + String(tmstruct.tm_sec));

      WebServerHandler();

      // Sart update server
      //DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "http://loraboatmonitorwebserverdata.derguntmar.de");
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
      DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");
      httpServer.begin();
      DebugPrint(3, "HTTP Update Server started at port: ");
      DebugPrintln(3, actconf.httpport);
      DebugPrint(3, "Use this URL: ");
      DebugPrint(3, "http://");
      DebugPrint(3, WiFi.softAPIP());
      DebugPrintln(3, "/update");
      DebugPrintln(3, "");
      
      // Start the NMEA TCP server
      server.begin();
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
      Timer3.attach_ms(SendPeriod, sendNMEA);    // Data transmission timer for NMEA

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
    int rawvoltage = 0;
    char rc;
    String receivedChars;

    if (Serial1.available()) {
      rc = Serial1.read();
      receivedChars += String(rc);
      // Read actual voltage
      if (receivedChars == "V"){
        rawvoltage = Serial1.parseInt();
        if(rawvoltage > 712){
          vedirectVoltage = rawvoltage / 1000.0;
          if (debugPrintValues) {
            DebugPrintln(3, "VE.direct V: " + String(vedirectVoltage, 3));
            
          }
          receivedChars = "";
        }
      }
      // Read actual current
      if (receivedChars == "I"){
        vedirectCurrent = Serial1.parseInt() / 1000.0;
        if (debugPrintValues) {
          DebugPrintln(3, "VE.direct I: " + String(vedirectCurrent));
        }
        receivedChars = "";
      }
      // Read actual power
      if (receivedChars == "P"){
        vedirectPower = Serial1.parseInt();
        if (debugPrintValues) {
          DebugPrintln(3, "VE.direct P: " + String(vedirectPower));
        }
        receivedChars = "";
      }
      // Read actual SOC
      if (receivedChars == "S"){
        vedirectSOC = Serial1.parseInt();
        if (debugPrintValues) {
          DebugPrintln(3, "VE.direct SOC: " + String(vedirectSOC));
        }
        receivedChars = "";
      }
      // Read actual temperature
      if (receivedChars == "T"){
        vedirectTemp = Serial1.parseInt() / 10.0;
        if (debugPrintValues) {
          DebugPrintln(3, "VE.direct T: " + String(vedirectTemp));
        }
        receivedChars = "";
      }
      // If line end then clear lina data
      if (rc == '\n'){
        receivedChars = "";
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
  }

  DebugPrintln(3, "LMIC.seqnoUp: " + String(LMIC.seqnoUp));
  LMIC.seqnoUp = actconf.fcnt;
  DebugPrintln(3, "After LMIC.seqnoUp = actconf.fcnt: " + String(LMIC.seqnoUp));
  DebugPrintln(3, "LMIC.seqnoUp: " + String(LMIC.seqnoUp));
  DebugPrintln(3, "actconf.fcnt: " + String(actconf.fcnt));

  DebugPrintln(3, "LoraWANDebug: ");
  LoraWANDebug(LMIC);
  //os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
  if (!lora_activ) {
    do_send(&sendjob);
  }
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
    // disable gps

    u8x8.clearDisplay();
    u8x8.setFont(u8x8_font_chroma48medium8_r);
    u8x8.drawString(0,0,"State 0   ");
    u8x8.drawString(0,1,"Batt switch off");
    u8x8.drawString(0,2,"Lora mode");
    u8x8.refreshDisplay();    // Only required for SSD1606/7

    if (String(actconf.loraOperationMode) == "Standby" || String(actconf.loraOperationMode) == "Always") {
      sendLoraQueue = true;
    }

    if (String(actconf.WifiStandbyMode) == "Yes") {
      enableWiFi();
    }
    lora_init();
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
        GoDeepSleep();
      }
    }
  }
  if (String(actconf.WifiStandbyMode) == "Yes") {
    if (String(actconf.SendDataViaWifi) == "Yes") {
      sendToMDS(actconf);
    }
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

    if (String(actconf.loraOperationMode) == "Always" || String(actconf.loraOperationMode) == "PowerOn") {
      Timer4.attach_ms((1000 * TX_INTERVAL), &onTimer);      // Start timer4 for sending Lora
      sendLoraQueue = true;
    } else {
      Timer4.detach();
    }
    writeDisplay();
    lora_init();
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

  if (state1counter >= 300000) {
    if (String(actconf.SendDataViaWifi) == "Yes") {
      sendToMDS(actconf);
    }
    state1counter = 0;
  }
  state1counter++;
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
  // While TCP client is connected or Serial Mode is active
  while ((client.connected() && !client.available()) || (int(actconf.serverMode) == 1)) {
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

  if (runDownloadingFiles) {
    runDownloadingFilesStatus = true;
    DownloadFilesFromWeb(actconf.fversion);
    runDownloadingFiles = false;
    runDownloadingFilesStatus = false;
  }

  if(reboot){
    DebugPrintln(3, "Reboot");
    ESP.restart(); // Restart ESP32
  }
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
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

void setup() {
  // Read the first byte from the EEPROM
  EEPROM.begin(sizeEEPROM);
  value = EEPROM.read(cfgStart);
  EEPROM.end();
  // If the fist Byte not identical to the first value in the default configuration then saving a default configuration in EEPROM.
  // Means if the EEPROM empty then saving a default configuration.
  if(value == defconf.valid){
    empty = 0;                  // Marker for configuration is present
  }
  else{
    saveEEPROMConfig(defconf);
    empty = 1;                  // Marker for configuration is missing
  }

  // Loading EEPROM configuration
  actconf = loadEEPROMConfig(); // Overload with old EEPROM configuration by start. It is necessarry for serspeed

  // If the firmware version in EEPROM different to defconf then save the new version in EEPROM
  if(String(actconf.fversion) != String(defconf.fversion)){
    String fver = defconf.fversion;
    fver.toCharArray(actconf.fversion, 6);
    saveEEPROMConfig(actconf);
  }

  if (actconf.WebSerialDebug == 1) {
    WebSerial.begin(&httpServer);

    /* Attach Message Callback */
    WebSerial.onMessage([&](uint8_t *data, size_t len) {
      DebugPrintln(3, "Received " + String(len) + " bytes from WebSerial: ");
      //Serial.write(data, len);
      //WebSerial.write(data, len);
      //DebugPrint(3, "Received Data: ");
      String d = "";
      for(size_t i=0; i < len; i++){
        d += char(data[i]);
      }
      
      DebugPrintln(3, "Received Data: " + String(d));
    });
  }

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

  // Loading EEPROM config
  DebugPrintln(3, "Loading actual EEPROM config");
  actconf = loadEEPROMConfig();
  DebugPrintln(3, "");

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
  esp_sleep_enable_timer_wakeup((TIME_TO_SLEEP * 60) * uS_TO_S_FACTOR);
  //DebugPrintln(3, "Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  if (String(actconf.standbyMode) == "On") {
    rtc_gpio_pullup_en(GPIO_NUM_39);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39,0);
  }

  readValues(actconf);     // initial read after boot, to get the status of alarm pin.

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    DebugPrintln(1, "LittleFS Mount Failed");
    return;
  }

  sendedLoraAfterSleepOneTime = false;
}

void loop() {
  if ((alarm1 == true) || (String(actconf.standbyMode) == "Off")) {
    state1();
  }

  if ((alarm1 == false) && (String(actconf.standbyMode) == "On")) {
    state0();
  }
  delay(20);
}
