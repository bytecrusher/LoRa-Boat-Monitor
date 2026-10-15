#pragma once
#include <Arduino.h>

// Lightweight, read-only GPS diagnostics for the existing Serial2 NMEA stream.
// Feed each received character to ingest(). No UART reads are performed here.
class GpsDiagnostics {
public:
  void ingest(char c, uint32_t nowMs = millis()) {
    if (c == '\r') return;
    if (c != '\n') {
      if (length_ < sizeof(line_) - 1) line_[length_++] = c;
      else length_ = 0;
      return;
    }
    line_[length_] = 0;
    length_ = 0;
    if (!validChecksum(line_)) { ++badChecksums_; return; }
    ++validSentences_;
    lastSentenceMs_ = nowMs;
    String sentence(line_);
    if (sentence.indexOf("GGA,") == 3) {
      fixQuality_ = field(sentence, 6).toInt();
      satellitesUsed_ = field(sentence, 7).toInt();
      hdop_ = field(sentence, 8).toFloat();
      lastGgaMs_ = nowMs;
      if (fixQuality_ > 0 && !everFixed_) {
        everFixed_ = true;
        firstFixMs_ = nowMs - startedMs_;
      }
    } else if (sentence.indexOf("RMC,") == 3) {
      rmcValid_ = field(sentence, 2) == "A";
      lastRmcMs_ = nowMs;
    }
  }

  void start(uint32_t nowMs = millis()) {
    startedMs_ = nowMs;
    everFixed_ = false;
    firstFixMs_ = 0;
  }

  String json(uint32_t nowMs = millis()) const {
    bool fresh = lastGgaMs_ && (nowMs - lastGgaMs_ < 10000UL);
    String s = "{";
    s += "\"nmeaFresh\":" + String(fresh ? "true" : "false");
    s += ",\"fix\":" + String(fresh && fixQuality_ > 0 ? "true" : "false");
    s += ",\"fixQuality\":" + String(fresh ? fixQuality_ : 0);
    s += ",\"satellitesUsed\":" + String(fresh ? satellitesUsed_ : 0);
    s += ",\"hdop\":" + String(fresh ? hdop_ : 0.0f, 2);
    s += ",\"rmcValid\":" + String(lastRmcMs_ && nowMs - lastRmcMs_ < 10000UL && rmcValid_ ? "true" : "false");
    s += ",\"timeToFirstFixMs\":" + (everFixed_ ? String(firstFixMs_) : "null");
    s += ",\"validSentences\":" + String(validSentences_);
    s += ",\"badChecksums\":" + String(badChecksums_);
    s += ",\"lastSentenceAgeMs\":" + (lastSentenceMs_ ? String(nowMs - lastSentenceMs_) : "null");
    s += "}";
    return s;
  }

private:
  static String field(const String &s, int index) {
    int begin = 0;
    for (int i = 0; i < index; ++i) {
      int comma = s.indexOf(',', begin);
      if (comma < 0) return "";
      begin = comma + 1;
    }
    int end = s.indexOf(',', begin);
    if (end < 0) end = s.indexOf('*', begin);
    return end < 0 ? "" : s.substring(begin, end);
  }
  static bool validChecksum(const char *s) {
    if (s[0] != '$') return false;
    const char *star = strchr(s, '*');
    if (!star || !star[1] || !star[2]) return false;
    uint8_t checksum = 0;
    for (const char *p = s + 1; p < star; ++p) checksum ^= uint8_t(*p);
    char hex[3] = {star[1], star[2], 0};
    return checksum == strtoul(hex, nullptr, 16);
  }
  char line_[128] = {};
  size_t length_ = 0;
  uint32_t startedMs_ = 0, firstFixMs_ = 0, lastSentenceMs_ = 0, lastGgaMs_ = 0, lastRmcMs_ = 0;
  uint32_t validSentences_ = 0, badChecksums_ = 0;
  int fixQuality_ = 0, satellitesUsed_ = 0;
  float hdop_ = 0;
  bool rmcValid_ = false, everFixed_ = false;
};
