#ifndef _func_myFunctions_H
#define _func_myFunctions_H

#include <Arduino.h>            // Arduino Environment
#include <ESPAsyncWebServer.h>  // asynchron webserver lib
#include <WebSerial.h>
#include <EEPROM.h>             // EEPROM lib
#include "MD5Builder.h"
#include <Adafruit_BME280.h>    // BME280
#include <OneWire.h>            // 1Wire lib
#include <DallasTemperature.h>  // DS18B20 lib
#include <U8x8lib.h>            // OLED Lib
#include <LittleFS.h>
#include <Configuration.h>

#define SEALEVELPRESSURE_HPA (1023.0) // Adjustment for altitude calculation
#define RXD2 12               // GPS TX GPIO12
#define TXD2 13               // GPS RX GPIO13

extern configData actconf;
extern int sizeEEPROM;
extern int cfgStart;
extern String transactionID;
extern String md5crypt;
extern int ledPin;
extern String raw;
extern int sf;
extern float fieldstrength;
extern float quality;
extern float temperature;
extern Adafruit_BME280 bme;
extern float pressure;
extern float humidity;
extern float altitude;
extern float dewp;
extern float vedirectVoltage;
extern float vedirectCurrent;
extern float vedirectTemp;
extern float voltage;
extern float temp1wire;
extern int ANALOG_IN;
extern float capacity;
extern float tank1;
extern int TANK1_IN;
extern uint16_t tank1adc;
extern float tank1p;
extern float tank2;
extern int TANK2_IN;
extern uint16_t tank2adc;
extern float tank2p;
extern int alarm1;
extern int alarmPin;
//#define OneWIRE_PIN 23
extern OneWire DS18B20;
extern DallasTemperature sensors;
extern float temp1wireold;
extern unsigned TX_INTERVAL;
extern float latitude;
extern float longitude;
extern U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8;
extern volatile bool flag2;
extern bool rmc_finish;
extern int c_counter;
extern bool lora_activ;
extern bool loraEvent_activ;
extern int inByte;
extern int start;
extern String nmea_all;
extern String nmea;
extern String gpsStatus;
extern String latitudeNS;
extern String longitudeEW;
extern int hour;
extern int minute;
extern int second;
extern int day;
extern int month;
extern int year;
extern int gpsspeed;
extern int course;
extern int relaytimer;
extern int relayPin;
extern volatile bool flag1;
extern int getRMC_hour(String s);
extern int getRMC_min(String s);
extern int getRMC_sec(String s);
extern String getRMC_status(String s);
extern float getRMC_Speed(String s);
extern float getRMC_Course(String s);
extern int getRMC_Day(String s);
extern int getRMC_Month(String s);
extern int getRMC_Year(String s);
extern float getRMC_LatDec(String s);
extern String getRMC_LatNS(String s);
extern float getRMC_LonDec(String s);
extern String getRMC_LonEW(String s);
extern String readFile2(fs::FS &fs, const char * path);

void DebugPrintln(int type, const char* x);
void DebugPrintln(int type, char x[]);
void DebugPrintln(int type, float x);
void DebugPrintln(int type, char x);
void DebugPrintln(int type, int x);
void DebugPrintln(int type, uint32_t x);
void DebugPrintln(int type, unsigned long x);
void DebugPrintln(int type, String x);
void DebugPrintln(int type, IPAddress x);
void DebugPrint(int type, const char* x);
void DebugPrint(int type, char x[]);
void DebugPrint(int type, float x);
void DebugPrint(int type, char x);
void DebugPrint(int type, int x);
void DebugPrint(int type, uint32_t x);
void DebugPrint(int type, unsigned long x);
void DebugPrint(int type, String x);
void DebugPrint(int type, IPAddress x);
void DebugPrint(int type, unsigned int num, int base);
void eraseEEPROMConfig(configData cfg);
void saveEEPROMConfig(configData cfg);
configData loadEEPROMConfig();
int boolToInt(bool value);
int toInteger(String settingValue);
float toFloat(String settingValue);
long toLong(String settingValue);
boolean toBoolean(String settingValue);
char* toChar(String command);
int HexToInt(char str[]);
int getindex(String data[], String compare);
String wlansymbol();
int wlanquality();
float truncate1(float value);
float truncate2(float value);
String firstzero(int value);
String transID();
String cryptPassword(String password);
int encryptPassword(String password, String md5hash);
void Serial1Clear();
void Serial2Clear();
uint16_t float2int(float value);
uint16_t float3int(float value);
uint16_t float4int(float value);
void flashLED(int duration);
float dewpoint(float temp, float humidity);
byte BinCheckSum(byte Data[]);
char CheckSum(String NMEAData);
bool CheckNMEA(String NMEAstring);
void readValues(configData myactconf);
void writeDisplay();
void writeDisplayValues(configData myactconf);
void readGPSValuesFlag();
void readGPSValues(configData myactconf);
void relayTimerInterrupt();
void sendNMEA();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
String humanReadableSize(const size_t bytes);
String getheader(configData myactconf);
void printLocalTime();

#endif