#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <Arduino.h>
#include <functional>
#include <ESPAsyncWebServer.h>

class WebSerialClass {
 public:
  using MessageCallback = std::function<void(uint8_t *data, size_t len)>;

  void begin(AsyncWebServer *server);
  void onMessage(MessageCallback callback);
  void loop();
  size_t write(const uint8_t *buffer, size_t size);
  size_t print(const char *value);
  size_t print(char *value);
  size_t print(char value);
  size_t print(int value);
  size_t print(unsigned int value, int base = DEC);
  size_t print(unsigned long value);
  size_t print(float value);
  size_t print(const String &value);
  size_t print(const IPAddress &value);
  size_t println(const char *value);
  size_t println(char *value);
  size_t println(char value);
  size_t println(int value);
  size_t println(unsigned int value);
  size_t println(unsigned long value);
  size_t println(float value);
 size_t println(const String &value);
  size_t println(const IPAddress &value);

 private:
  void append(const String &value);
  MessageCallback callback_;
  AsyncEventSource *events_ = nullptr;
  String logBuffer_;
  bool initialized_ = false;
};

extern WebSerialClass WebSerial;

#endif
