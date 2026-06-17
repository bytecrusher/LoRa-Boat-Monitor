#include "func_webServerHandler.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include "func_webclient.h"

extern const uint8_t cert_cacert_pem_start[] asm("_binary_cert_cacert_pem_start");

String csrfToken = "";

String getCsrfToken() {
  if (csrfToken.length() == 0) {
    csrfToken = String(esp_random(), HEX) + String(esp_random(), HEX) + String(esp_random(), HEX);
  }

  return csrfToken;
}

bool isCsrfTokenValid(AsyncWebServerRequest *request) {
  const String expectedToken = getCsrfToken();
  if (request->hasHeader("X-CSRF-Token") && request->header("X-CSRF-Token") == expectedToken) {
    return true;
  }

  if (request->hasParam("csrf", true) && request->getParam("csrf", true)->value() == expectedToken) {
    return true;
  }

  if (request->hasParam("csrf", true, true) && request->getParam("csrf", true, true)->value() == expectedToken) {
    return true;
  }

  return false;
}

bool requireCsrfToken(AsyncWebServerRequest *request) {
  if (isCsrfTokenValid(request)) {
    return true;
  }

  request->send(403, "application/json", "{\"status\":\"error\",\"message\":\"Invalid CSRF token.\"}");
  return false;
}

String maskSecret(const String &secret) {
  if (secret.length() == 0) {
    return "";
  }

  if (secret.length() <= 4) {
    return "****";
  }

  return secret.substring(0, 2) + "****" + secret.substring(secret.length() - 2);
}

namespace {
String buildOtaProgressJson();
String buildUpdateFilesProgressJson();
void startOtaProgress(const String &phase, size_t totalBytes, const String &message);
void finishOtaProgress(bool success, const String &message);

String htmlEscape(const String &value) {
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

int clampConfigInt(int value, int fallback, int minValue, int maxValue) {
  if (value < minValue || value > maxValue) {
    return fallback;
  }

  return value;
}

unsigned int clampConfigUInt(unsigned int value, unsigned int fallback, unsigned int minValue, unsigned int maxValue) {
  if (value < minValue || value > maxValue) {
    return fallback;
  }

  return value;
}

String formatHexKey(const uint8_t *key, size_t length) {
  String value = "";
  for (size_t i = 0; i < length; i++) {
    String part = String(key[i], HEX);
    if (part.length() == 1) {
      part = "0" + part;
    }
    part.toUpperCase();
    value += part;
  }
  return value;
}

bool parseHexKey(const String &value, uint8_t *target, size_t targetLength) {
  if (value.length() != targetLength * 2) {
    return false;
  }

  char hexstring[3];
  hexstring[2] = '\0';
  for (size_t j = 0; j < targetLength; j++) {
    const char high = value[j * 2];
    const char low = value[j * 2 + 1];
    if (!isxdigit(high) || !isxdigit(low)) {
      return false;
    }
    hexstring[0] = high;
    hexstring[1] = low;
    target[j] = HexToInt(hexstring);
  }
  return true;
}

String normalizeFirmwareUpdateBaseUrl(const String &configuredValue) {
  String normalized = configuredValue;
  normalized.trim();

  if (normalized.startsWith("https://")) {
    normalized.remove(0, 8);
  } else if (normalized.startsWith("http://")) {
    normalized.remove(0, 7);
  }

  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }

  const String suffix = "/files_for_esp_webserver";
  if (normalized.endsWith(suffix)) {
    normalized.remove(normalized.length() - suffix.length());
  }

  return "https://" + normalized + suffix;
}

String normalizeFirmwareUpdateValueForStorage(const String &configuredValue) {
  String normalized = configuredValue;
  normalized.trim();

  if (normalized.startsWith("https://")) {
    normalized.remove(0, 8);
  } else if (normalized.startsWith("http://")) {
    normalized.remove(0, 7);
  }

  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }

  const String suffix = "/files_for_esp_webserver";
  if (normalized.endsWith(suffix)) {
    normalized.remove(normalized.length() - suffix.length());
  }

  const int slashIndex = normalized.indexOf('/');
  if (slashIndex >= 0) {
    normalized = normalized.substring(0, slashIndex);
  }

  return normalized;
}

bool isPrintableAscii(const String &value) {
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value.charAt(i);
    if (c < 32 || c > 126) {
      return false;
    }
  }
  return true;
}

bool isDefaultWebPasswordActive() {
  return strcmp(actconf.password, defconf.password) == 0;
}

bool requireNonDefaultWebPassword(AsyncWebServerRequest *request) {
  if (!isDefaultWebPasswordActive()) {
    return true;
  }

  request->send(428, "application/json", "{\"status\":\"error\",\"message\":\"Please change the default web password before using this high-risk action.\"}");
  return false;
}

String normalizeMdsOtaUrlForStorage(const String &configuredValue) {
  String normalized = configuredValue;
  normalized.trim();

  if (!isPrintableAscii(normalized) || !normalized.startsWith("https://")) {
    return String(defconf.mdsOtaUrl);
  }

  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }

  return normalized;
}

bool fetchHttpsText(const String &url, String &payload, String *errorMessage = nullptr, uint16_t timeoutMs = 10000) {
  WiFiClientSecure client;
  HTTPClient http;

  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(timeoutMs);

  if (!http.begin(client, url)) {
    if (errorMessage != nullptr) {
      *errorMessage = "Unable to initialize HTTPS request";
    }
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    if (errorMessage != nullptr) {
      if (httpCode > 0) {
        *errorMessage = "HTTP " + String(httpCode);
      } else {
        *errorMessage = http.errorToString(httpCode);
      }
    }
    http.end();
    return false;
  }

  payload = http.getString();
  payload.trim();
  http.end();
  if (payload.length() == 0 && errorMessage != nullptr) {
    *errorMessage = "Empty response";
  }
  return payload.length() > 0;
}

String extractVersionFromFirmwareUrl(const String &url) {
  const int firmwareIndex = url.lastIndexOf("/firmware.bin");
  if (firmwareIndex < 0) {
    return String();
  }

  const int versionEnd = firmwareIndex;
  const int versionStart = url.lastIndexOf('/', versionEnd - 1);
  if (versionStart < 0 || versionStart + 1 >= versionEnd) {
    return String();
  }

  return url.substring(versionStart + 1, versionEnd);
}

String getConfiguredFirmwareBaseUrl() {
  return normalizeFirmwareUpdateBaseUrl(String(actconf.firmwareUpdateUrl));
}

String resolveFirmwareUrlFromManifestValue(const String &baseUrl, const String &manifestValue) {
  String value = manifestValue;
  value.trim();

  if (value.startsWith("https://")) {
    const String allowedPrefix = baseUrl + "/";
    return value.startsWith(allowedPrefix) ? value : String();
  }

  while (value.startsWith("/")) {
    value.remove(0, 1);
  }

  return baseUrl + "/" + value;
}

bool resolveFirmwareFromManifest(const String &channel, String &firmwareUrl, String &version, String *errorMessage = nullptr, String *sha256 = nullptr) {
  const String baseUrl = getConfiguredFirmwareBaseUrl();
  String payload;
  if (!fetchHttpsText(baseUrl + "/firmware-manifest.json", payload, errorMessage)) {
    return false;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, payload);
  if (parseError) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest parse failed";
    }
    return false;
  }

  JsonObject release = doc[channel.c_str()].as<JsonObject>();
  if (release.isNull()) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest channel missing";
    }
    return false;
  }

  version = release["version"].as<String>();
  firmwareUrl = resolveFirmwareUrlFromManifestValue(baseUrl, release["firmware"].as<String>());
  if (sha256 != nullptr) {
    *sha256 = release["sha256"].as<String>();
    sha256->trim();
    sha256->toLowerCase();
  }

  if (version.length() == 0 || firmwareUrl.length() == 0) {
    if (errorMessage != nullptr) {
      *errorMessage = "Manifest release incomplete";
    }
    return false;
  }

  return true;
}

int compareFirmwareVersions(const String &leftVersion, const String &rightVersion) {
  int leftMajor = 0;
  int leftMinor = 0;
  char leftSuffix = '\0';
  int rightMajor = 0;
  int rightMinor = 0;
  char rightSuffix = '\0';

  sscanf(leftVersion.c_str(), "V%d.%d%c", &leftMajor, &leftMinor, &leftSuffix);
  sscanf(rightVersion.c_str(), "V%d.%d%c", &rightMajor, &rightMinor, &rightSuffix);

  if (leftMajor != rightMajor) {
    return leftMajor > rightMajor ? 1 : -1;
  }

  if (leftMinor != rightMinor) {
    return leftMinor > rightMinor ? 1 : -1;
  }

  if (leftSuffix == rightSuffix) {
    return 0;
  }

  if (leftSuffix == '\0') {
    return -1;
  }

  if (rightSuffix == '\0') {
    return 1;
  }

  return leftSuffix > rightSuffix ? 1 : -1;
}

String normalizeSha256(const String &sha256) {
  String normalized = sha256;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized.length() != 64) {
    return "";
  }

  for (size_t i = 0; i < normalized.length(); i++) {
    const char c = normalized.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return "";
    }
  }

  return normalized;
}

String sha256ToHex(const uint8_t digest[32]) {
  static const char hexChars[] = "0123456789abcdef";
  String hex;
  hex.reserve(64);

  for (uint8_t i = 0; i < 32; i++) {
    hex += hexChars[(digest[i] >> 4) & 0x0F];
    hex += hexChars[digest[i] & 0x0F];
  }

  return hex;
}

bool fetchMdsOtaInfo(JsonDocument &response) {
  response["configured"] = strlen(actconf.mdsOtaSecret) > 0 && String(actconf.mdsOtaUrl).startsWith("https://");
  response["installedVersion"] = String(actconf.fversion);
  response["version"] = "";
  response["status"] = "not-configured";
  response["message"] = "MDS OTA endpoint or secret is not configured.";

  if (!response["configured"].as<bool>()) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    response["status"] = "offline";
    response["message"] = "No Wi-Fi connection available.";
    return false;
  }

  WiFiClientSecure client;
  HTTPClient http;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(8000);

  const char *responseHeaders[] = {
    "x-Firmware-Version",
    "X-Firmware-Version",
    "x-ESP32-version",
    "X-ESP32-Version",
    "x-SHA256",
    "X-SHA256"
  };
  http.collectHeaders(responseHeaders, 6);
  http.setUserAgent("ESP32-http-Update");

  String endpoint = String(actconf.mdsOtaUrl);
  endpoint.trim();
  if (!http.begin(client, endpoint)) {
    response["status"] = "error";
    response["message"] = "Unable to initialize MDS OTA status request.";
    return false;
  }

  http.addHeader("X-MDS-OTA-Secret", String(actconf.mdsOtaSecret));
  http.addHeader("x-ESP32-STA-MAC", WiFi.macAddress());
  http.addHeader("x-ESP32-sketch-md5", ESP.getSketchMD5());
  http.addHeader("x-ESP32-sdk-version", String(ESP.getSdkVersion()));
  http.addHeader("x-ESP32-version", String(actconf.fversion));
  http.addHeader("X-MDS-OTA-Check", "1");

  const int httpCode = http.GET();
  response["httpStatus"] = httpCode;

  String version = http.header("x-Firmware-Version");
  if (version.length() == 0) {
    version = http.header("X-Firmware-Version");
  }
  if (version.length() == 0) {
    version = http.header("x-ESP32-version");
  }
  if (version.length() == 0) {
    version = http.header("X-ESP32-Version");
  }
  version.trim();
  response["version"] = version;

  String sha256 = normalizeSha256(http.header("x-SHA256"));
  if (sha256.length() == 0) {
    sha256 = normalizeSha256(http.header("X-SHA256"));
  }
  response["sha256Available"] = sha256.length() == 64;

  if (httpCode == HTTP_CODE_OK) {
    response["status"] = "update-available";
    response["message"] = version.length() ? "MDS firmware update available." : "MDS firmware update available, but no version header was returned.";
  } else if (httpCode == HTTP_CODE_NOT_MODIFIED) {
    response["status"] = "current";
    response["message"] = "MDS reports that the installed firmware is current.";
  } else if (httpCode == HTTP_CODE_FORBIDDEN) {
    response["status"] = "forbidden";
    response["message"] = "MDS rejected the OTA secret or request headers.";
  } else if (httpCode > 0) {
    response["status"] = "error";
    response["message"] = "MDS OTA status request failed: HTTP " + String(httpCode);
  } else {
    response["status"] = "error";
    response["message"] = "MDS OTA status request failed.";
  }

  http.end();
  return httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NOT_MODIFIED;
}

bool resolveStableFirmware(String &firmwareUrl, String &version, String *errorMessage = nullptr, String *sha256 = nullptr) {
  String manifestError;
  if (resolveFirmwareFromManifest("stable", firmwareUrl, version, &manifestError, sha256)) {
    return true;
  }

  const String baseUrl = getConfiguredFirmwareBaseUrl();
  const char *candidateFiles[] = {
    "latestVersion.txt",
    "latestStableVersion.txt",
    "latestFirmwareVersion.txt",
    "ActualVersion.txt"
  };

  String payload;
  String lastError;
  for (size_t i = 0; i < (sizeof(candidateFiles) / sizeof(candidateFiles[0])); ++i) {
    if (!fetchHttpsText(baseUrl + "/" + candidateFiles[i], payload, &lastError)) {
      continue;
    }

    if (payload.startsWith("https://")) {
      firmwareUrl = payload;
      version = extractVersionFromFirmwareUrl(payload);
      if (sha256 != nullptr) {
        *sha256 = "";
      }
      return firmwareUrl.length() > 0;
    }

    version = payload;
    firmwareUrl = baseUrl + "/" + version + "/firmware.bin";
    if (sha256 != nullptr) {
      *sha256 = "";
    }
    return version.length() > 0;
  }

  if (errorMessage != nullptr) {
    *errorMessage = lastError.length() ? lastError : (manifestError.length() ? manifestError : "No stable version marker found");
  }
  return false;
}

bool resolveBetaFirmware(String &firmwareUrl, String &version, String *errorMessage = nullptr, String *sha256 = nullptr) {
  String manifestError;
  if (resolveFirmwareFromManifest("beta", firmwareUrl, version, &manifestError, sha256)) {
    return true;
  }

  const String baseUrl = getConfiguredFirmwareBaseUrl();
  String payload;
  if (!fetchHttpsText(baseUrl + "/latestBetaVersion.txt", payload, errorMessage)) {
    return false;
  }

  if (payload.startsWith("https://")) {
    firmwareUrl = payload;
    version = extractVersionFromFirmwareUrl(payload);
    if (sha256 != nullptr) {
      *sha256 = "";
    }
    return firmwareUrl.length() > 0;
  }

  version = payload;
  firmwareUrl = baseUrl + "/" + version + "/firmware.bin";
  if (sha256 != nullptr) {
    *sha256 = "";
  }
  return version.length() > 0;
}

String settingsTemplateProcessor(const String &var) {
  if (var == "header") return getheader(actconf);
  if (var == "devname") return htmlEscape(String(actconf.devname));
  if (var == "cssid1") return htmlEscape(String(actconf.cssid1));
  if (var == "cpassword1") return htmlEscape(String(actconf.cpassword1));
  if (var == "cpassword1Masked") return maskSecret(String(actconf.cpassword1));
  if (var == "cssid2") return htmlEscape(String(actconf.cssid2));
  if (var == "cpassword2") return htmlEscape(String(actconf.cpassword2));
  if (var == "cpassword2Masked") return maskSecret(String(actconf.cpassword2));
  if (var == "cssid3") return htmlEscape(String(actconf.cssid3));
  if (var == "cpassword3") return htmlEscape(String(actconf.cpassword3));
  if (var == "cpassword3Masked") return maskSecret(String(actconf.cpassword3));
  if (var == "username") return htmlEscape(String(actconf.username));
  if (var == "password") return htmlEscape(String(actconf.password));
  if (var == "passwordMasked") return maskSecret(String(actconf.password));
  if (var == "SendDataViaWifi") return String(getindex(SendDataViaWifi, String(actconf.SendDataViaWifi)));
  if (var == "firmwareUpdateUrl") return htmlEscape(String(actconf.firmwareUpdateUrl));
  if (var == "mdsOtaUrl") return htmlEscape(String(actconf.mdsOtaUrl));
  if (var == "mdsOtaSecretMasked") return maskSecret(String(actconf.mdsOtaSecret));
  if (var == "csrfToken") return getCsrfToken();

  if (var == "MdsUrl") return htmlEscape(String(actconf.MdsUrl));
  if (var == "MdsApiKey") return htmlEscape(String(actconf.MdsApiKey));
  if (var == "MdsApiKeyMasked") return maskSecret(String(actconf.MdsApiKey));
  if (var == "MdsSensorIdBattery") return String(actconf.MdsSensorIdBattery);
  if (var == "MdsSensorIdTanks") return String(actconf.MdsSensorIdTanks);
  if (var == "MdsSensorIdStatus") return String(actconf.MdsSensorIdStatus);
  if (var == "MdsSensorIdGps") return String(actconf.MdsSensorIdGps);
  if (var == "MdsSensorIdEnv") return String(actconf.MdsSensorIdEnv);
  if (var == "MdsSensorIdDewpoint") return String(actconf.MdsSensorIdDewpoint);
  if (var == "MdsSensorIdVedirect") return String(actconf.MdsSensorIdVedirect);

  if (var == "hostname") return htmlEscape(String(actconf.hostname));
  if (var == "sssid") return htmlEscape(String(actconf.sssid));
  if (var == "spassword") return htmlEscape(String(actconf.spassword));
  if (var == "spasswordMasked") return maskSecret(String(actconf.spassword));

  if (var == "crypt") return String(getindex(usepassword, String(actconf.crypt)));
  if (var == "instrumentSize") return String(getindex(isize, String(actconf.instrumentSize)));
  if (var == "timeout") return String(getindex(timeout, String(actconf.timeout)));
  if (var == "apchannel") return String(getindex(apchannel, String(actconf.apchannel)));
  if (var == "serverMode") return String(getindex(servermode, String(actconf.serverMode)));
  if (var == "mDNS") return String(getindex(mdnsservice, String(actconf.mDNS)));

  if (var == "lorafrequency") return String(getindex(lorafrequencys, String(actconf.lorafrequency)));
  if (var == "lchannel") return String(getindex(lchannel, String(actconf.lchannel)));
  if (var == "spreadf") return String(getindex(spreadf, String(actconf.spreadf)));
  if (var == "dynsf") return String(getindex(dynsf, String(actconf.dynsf)));
  if (var == "tinterval") return String(actconf.tinterval);
  if (var == "relay") return String(getindex(relay, String(actconf.relay)));

  if (var == "devaddr") {
    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    return mystring;
  }

  if (var == "nskey") {
    return "";
  }

  if (var == "nskeyMasked") {
    return maskSecret(formatHexKey(actconf.nskey, sizeof(actconf.nskey)));
  }

  if (var == "appkey") {
    return "";
  }

  if (var == "appkeyMasked") {
    return maskSecret(formatHexKey(actconf.appkey, sizeof(actconf.appkey)));
  }

  auto fmt5 = [](float value) {
    char buf[20];
    sprintf(buf, "%.5f", value);
    return String(buf);
  };

  if (var == "a1t1slope") return fmt5(actconf.a1t1slope);
  if (var == "a2t1slope") return fmt5(actconf.a2t1slope);
  if (var == "t1offset") return fmt5(actconf.t1offset);
  if (var == "a2t2slope") return fmt5(actconf.a2t2slope);
  if (var == "a1t2slope") return fmt5(actconf.a1t2slope);
  if (var == "t2offset") return fmt5(actconf.t2offset);

  if (var == "voffset") return fmt5(actconf.voffset);
  if (var == "a2vslope") return fmt5(actconf.a2vslope);
  if (var == "a1vslope") return fmt5(actconf.a1vslope);

  if (var == "debug") return String(getindex(debugmode, String(actconf.debug)));
  if (var == "serspeed") return String(getindex(serspeed, String(actconf.serspeed)));
  if (var == "WebSerialDebug") return String(getindex(WebSerialDebug, String(actconf.WebSerialDebug)));
  if (var == "deviceID") return String(getindex(deviceid, String(actconf.deviceID)));
  if (var == "senddata") return String(getindex(senddata, String(actconf.senddata)));
  if (var == "vaverage") return String(getindex(vaverage, String(actconf.vaverage)));
  if (var == "t1average") return String(getindex(t1average, String(actconf.t1average)));
  if (var == "t2average") return String(getindex(t2average, String(actconf.t2average)));
  if (var == "tempSensorType") return String(getindex(tstype, String(actconf.tempSensorType)));
  if (var == "tempUnit") return String(getindex(tempunits, String(actconf.tempUnit)));
  if (var == "envSensor") return String(getindex(envSensor, String(actconf.envSensor)));
  if (var == "standbyMode") return String(getindex(standbyMode, String(actconf.standbyMode)));
  if (var == "standbySleepDuration") return String(actconf.standbySleepDuration);
  if (var == "loraOperationMode") return String(getindex(loraOperationMode, String(actconf.loraOperationMode)));
  if (var == "WifiStandbyMode") return String(getindex(WifiStandbyMode, String(actconf.WifiStandbyMode)));
  if (var == "cssStyle") return String(getindex(cssStyle, String(actconf.cssStyle)));
  if (var == "OledDisplayRotation") return String(getindex(OledDisplayRotation, String(actconf.OledDisplayRotation)));

  return String();
}

String buildOtaResponse(const char *status, const String &message, bool rebooting, bool checkWebFiles, bool backupSaved) {
  String response = "{\"status\":\"";
  response += status;
  response += "\",\"message\":\"";
  response += message;
  response += "\",\"rebooting\":";
  response += rebooting ? "true" : "false";
  response += ",\"checkWebFiles\":";
  response += checkWebFiles ? "true" : "false";
  response += ",\"backupSaved\":";
  response += backupSaved ? "true" : "false";
  response += "}";
  return response;
}
}  // namespace

//const int relayPin = 25;      // Pin GPIO25, Relay is high activ

const char logout_html2[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>
    <body>
      <p>Logged out or <a href="/">return to homepage</a>.</p>
      <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
    </body>
  </html>
)rawliteral";

void WebServerHandler()
{
  auto handleSaveSettings = [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      DebugPrintln(2, "Blocked saving settings without a valid CSRF token.");
      return;
    }

    // Read all received get arguments and save in a array
    int num = request->args();
    String vname[num];
    String value[num];
    for (int i = 0; i < num; i++) {
      vname[i] = request->argName(i);
      value[i] = request->arg(i);  
    } 
    // Check new settings and save it in configuration
    for (int i = 0; i < num; i++)
    {
      // Passwort Settings
      //******************
      if (vname[i] == "usepassword") {
        actconf.crypt = 1;
      }
      if (vname[i] == "pagepasswd") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.password, sizeof(actconf.password));
        }
      }
      // Display Settings
      //*****************
      if (vname[i] == "isize") {
        actconf.instrumentSize = toInteger(value[i]);
      }
      // Network Settings
      //*****************
      if (vname[i] == "cssid1") {
        value[i].toCharArray(actconf.cssid1, sizeof(actconf.cssid1));
      }
      if (vname[i] == "cpasswd1") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.cpassword1, sizeof(actconf.cpassword1));
        }
      }
      if (vname[i] == "cssid2") {
        value[i].toCharArray(actconf.cssid2, sizeof(actconf.cssid2));
      }
      if (vname[i] == "cpasswd2") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.cpassword2, sizeof(actconf.cpassword2));
        }
      }
      if (vname[i] == "cssid3") {
        value[i].toCharArray(actconf.cssid3, sizeof(actconf.cssid3));
      }
      if (vname[i] == "cpasswd3") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.cpassword3, sizeof(actconf.cpassword3));
        }
      }
      if (vname[i] == "timeout") {
        actconf.timeout = clampConfigInt(toInteger(value[i]), defconf.timeout, 3, 240);
      }
      if (vname[i] == "sssid") {
        value[i].toCharArray(actconf.sssid, sizeof(actconf.sssid));
      }
      if (vname[i] == "spasswd") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.spassword, sizeof(actconf.spassword));
        }
      }
      if (vname[i] == "apchannel") {
        actconf.apchannel = clampConfigInt(toInteger(value[i]), defconf.apchannel, 1, 13);
      }
      if (vname[i] == "firmwareUpdateUrl") {
        String normalizedFirmwareUpdateUrl = normalizeFirmwareUpdateValueForStorage(value[i]);
        normalizedFirmwareUpdateUrl.toCharArray(actconf.firmwareUpdateUrl, sizeof(actconf.firmwareUpdateUrl));
      }
      if (vname[i] == "mdsOtaUrl") {
        String normalizedMdsOtaUrl = normalizeMdsOtaUrlForStorage(value[i]);
        normalizedMdsOtaUrl.toCharArray(actconf.mdsOtaUrl, sizeof(actconf.mdsOtaUrl));
      }
      if (vname[i] == "mdsOtaSecret") {
        if (value[i].length() > 0 && isPrintableAscii(value[i])) {
          value[i].toCharArray(actconf.mdsOtaSecret, sizeof(actconf.mdsOtaSecret));
        }
      }
      if (vname[i] == "servermode") {
        actconf.serverMode = clampConfigInt(toInteger(value[i]), defconf.serverMode, 0, 4);
      }
      if (vname[i] == "mdnsservice") {
        actconf.mDNS = clampConfigInt(toInteger(value[i]), defconf.mDNS, 0, 1);
      }
      if (vname[i] == "SendDataViaWifi") {
        value[i].toCharArray(actconf.SendDataViaWifi, sizeof(actconf.SendDataViaWifi));
      }
      if (vname[i] == "MdsUrl") {
        String mdsUrl = value[i];
        mdsUrl.trim();
        if (mdsUrl.length() > 0 && mdsUrl.indexOf("://") < 0) {
          mdsUrl = "https://" + mdsUrl;
        }
        if (mdsUrl.startsWith("https://") && mdsUrl.length() < sizeof(actconf.MdsUrl)) {
          mdsUrl.toCharArray(actconf.MdsUrl, sizeof(actconf.MdsUrl));
        }
      }
      if (vname[i] == "MdsApiKey") {
        if (value[i].length() > 0) {
          value[i].toCharArray(actconf.MdsApiKey, sizeof(actconf.MdsApiKey));
        }
      }
      if (vname[i] == "MdsSensorIdBattery") {
        actconf.MdsSensorIdBattery = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdBattery, 0, 1);
      }
      if (vname[i] == "MdsSensorIdTanks") {
        actconf.MdsSensorIdTanks = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdTanks, 0, 1);
      }
      if (vname[i] == "MdsSensorIdStatus") {
        actconf.MdsSensorIdStatus = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdStatus, 0, 1);
      }
      if (vname[i] == "MdsSensorIdGps") {
        actconf.MdsSensorIdGps = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdGps, 0, 1);
      }
      if (vname[i] == "MdsSensorIdEnv") {
        actconf.MdsSensorIdEnv = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdEnv, 0, 1);
      }
      if (vname[i] == "MdsSensorIdDewpoint") {
        actconf.MdsSensorIdDewpoint = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdDewpoint, 0, 1);
      }
      if (vname[i] == "MdsSensorIdVedirect") {
        actconf.MdsSensorIdVedirect = clampConfigInt(toInteger(value[i]), defconf.MdsSensorIdVedirect, 0, 1);
      }
      // LoRa Settings
      //**************
      if (vname[i] == "devaddr") {
        char hexstring[9];
        value[i].toCharArray(hexstring, sizeof(hexstring));
        actconf.devaddr = HexToInt(hexstring);
      }
      if (vname[i] == "nskey") {
        if (value[i].length() > 0) {
          if (parseHexKey(value[i], actconf.nskey, sizeof(actconf.nskey))) {
            DebugPrintln(3, "LoRa network session key updated");
          } else {
            DebugPrintln(2, "Invalid LoRa network session key ignored");
          }
        }
      }
      if (vname[i] == "appkey") {
        if (value[i].length() > 0) {
          if (parseHexKey(value[i], actconf.appkey, sizeof(actconf.appkey))) {
            DebugPrintln(3, "LoRa application session key updated");
          } else {
            DebugPrintln(2, "Invalid LoRa application session key ignored");
          }
        }
      } 
      if (vname[i] == "lorafrequency") {
        value[i].toCharArray(actconf.lorafrequency, sizeof(actconf.lorafrequency));
      }
      if (vname[i] == "lchannel") {
        actconf.lchannel = clampConfigInt(toInteger(value[i]), defconf.lchannel, 0, 9);
      }
      if (vname[i] == "dynsf") {
        actconf.dynsf = clampConfigInt(toInteger(value[i]), defconf.dynsf, 0, 1);
      }
      if (vname[i] == "spreadf") {
        actconf.spreadf = clampConfigInt(toInteger(value[i]), defconf.spreadf, 7, 10);
      }
      if (vname[i] == "tinterval") {
        const unsigned int newInterval = clampConfigUInt(toInteger(value[i]), defconf.tinterval, 1U, 255U);
        if(actconf.tinterval != newInterval){
          actconf.tinterval = newInterval;
          TX_INTERVAL = actconf.tinterval * 60;
        }
      }
      if (vname[i] == "relay") {
        actconf.relay = clampConfigInt(toInteger(value[i]), defconf.relay, 0, 2);
        if(actconf.relay == 0){
          digitalWrite(relayPin, LOW);
          relaytimer = 0;
        }
        else{
          digitalWrite(relayPin, HIGH);
        }
      }
      if (vname[i] == "debugmode") {
        actconf.debug = clampConfigInt(toInteger(value[i]), defconf.debug, 0, 3);
      }
      if (vname[i] == "serspeed") {
        actconf.serspeed = clampConfigInt(toInteger(value[i]), defconf.serspeed, 300, 115200);
      }
      if (vname[i] == "WebSerialDebug") {
        actconf.WebSerialDebug = clampConfigInt(toInteger(value[i]), defconf.WebSerialDebug, 0, 1);
      }
      if (vname[i] == "deviceid") {
        actconf.deviceID = clampConfigInt(toInteger(value[i]), defconf.deviceID, 0, 9);
      }
      if (vname[i] == "senddata") {
        actconf.senddata = clampConfigInt(toInteger(value[i]), defconf.senddata, 0, 1);
      }    
      if (vname[i] == "vaverage") {
        actconf.vaverage = clampConfigInt(toInteger(value[i]), defconf.vaverage, 1, 100);
      }
      if (vname[i] == "t1average") {
        actconf.t1average = clampConfigInt(toInteger(value[i]), defconf.t1average, 1, 100);
      }
      if (vname[i] == "t2average") {
        actconf.t2average = clampConfigInt(toInteger(value[i]), defconf.t2average, 1, 100);
      }   
      if (vname[i] == "tstype") {
        value[i].toCharArray(actconf.tempSensorType, sizeof(actconf.tempSensorType));
      }
      if (vname[i] == "tempunit") {
        value[i].toCharArray(actconf.tempUnit, sizeof(actconf.tempUnit));
      }
      if (vname[i] == "envSensor") {
        value[i].toCharArray(actconf.envSensor, sizeof(actconf.envSensor));
      }
      if (vname[i] == "standbyMode") {
        value[i].toCharArray(actconf.standbyMode, sizeof(actconf.standbyMode));
      }
      if (vname[i] == "standbySleepDuration") {
        actconf.standbySleepDuration = clampConfigInt(toInteger(value[i]), defconf.standbySleepDuration, 1, 1440);
      }
      if (vname[i] == "loraOperationMode") {
        value[i].toCharArray(actconf.loraOperationMode, sizeof(actconf.loraOperationMode));
      }
      if (vname[i] == "WifiStandbyMode") {
        value[i].toCharArray(actconf.WifiStandbyMode, sizeof(actconf.WifiStandbyMode));
      }
      if (vname[i] == "a1vslope") {
        actconf.a1vslope = toFloat(value[i]);
      }
      if (vname[i] == "a2vslope") {
        actconf.a2vslope = toFloat(value[i]);
      }
      if (vname[i] == "voffset") {
        actconf.voffset = toFloat(value[i]);
      }
      if (vname[i] == "a1t1slope") {
        actconf.a1t1slope = toFloat(value[i]);
      }
      if (vname[i] == "a2t1slope") {
        actconf.a2t1slope = toFloat(value[i]);
      }
      if (vname[i] == "t1offset") {
        actconf.t1offset = toFloat(value[i]);
      }
      if (vname[i] == "a1t2slope") {
        actconf.a1t2slope = toFloat(value[i]);
      }
      if (vname[i] == "a2t2slope") {
        actconf.a2t2slope = toFloat(value[i]);
      }
      if (vname[i] == "t2offset") {
        actconf.t2offset = toFloat(value[i]);
      }
      if (vname[i] == "cssStyle") {
        actconf.cssStyle = clampConfigInt(toInteger(value[i]), defconf.cssStyle, 0, 2);
      }
      if (vname[i] == "OledDisplayRotation") {
        actconf.OledDisplayRotation = clampConfigInt(toInteger(value[i]), defconf.OledDisplayRotation, 0, 1);
      }
    }

    if(num > 0) {  
      saveEEPROMConfig(actconf);
      standbySleepBlockedUntilMillis = millis() + 30000UL;
      DebugPrintln(3, "New settings saved");
    }
    DebugPrintln(3, "Send settings.html");
    request->redirect("/settings.html");
  };

  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/index.html");
  });

  httpServer.on("/", HTTP_HEAD, [](AsyncWebServerRequest *request) {
    request->redirect("/index.html");
  });

  httpServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/index.html");
    //content.replace("%header%", String(readFile2(LittleFS, "/header.html")));
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));

    if (content == "- failed to open file for reading"){
      request->redirect("/initialsetup.html");
    } else {
      request->send(200, "text/html", content);
    }
  });

  httpServer.on("/index.html", HTTP_HEAD, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(200, "text/html", "");
    } else {
      request->redirect("/initialsetup.html");
    }
  });

  httpServer.on("/initialsetup.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String content = initialsetup_html;
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%crights%", htmlEscape(String(actconf.crights)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%cssid1%", htmlEscape(String(actconf.cssid1)));
    content.replace("%cpassword1%", "");
    content.replace("%cpassword1Masked%", maskSecret(String(actconf.cpassword1)));
    content.replace("%cssid2%", htmlEscape(String(actconf.cssid2)));
    content.replace("%cpassword2%", "");
    content.replace("%cpassword2Masked%", maskSecret(String(actconf.cpassword2)));
    content.replace("%cssid3%", htmlEscape(String(actconf.cssid3)));
    content.replace("%cpassword3%", "");
    content.replace("%cpassword3Masked%", maskSecret(String(actconf.cpassword3)));
    content.replace("%quality%", String(int(quality)));
    content.replace("%csrfToken%", getCsrfToken());
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));

    content.replace("%FREE_FILESYSTEM%", humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes())));
    content.replace("%USED_FILESYSTEM%", humanReadableSize(LittleFS.usedBytes()));
    content.replace("%TOTAL_FILESYSTEM%", humanReadableSize(LittleFS.totalBytes()));
    content.replace("%USED_FILESYSTEM_BYTES%", String(LittleFS.usedBytes()));
    content.replace("%TOTAL_FILESYSTEM_BYTES%", String(LittleFS.totalBytes()));

    request->send(200, "text/html", content);
  });

  httpServer.on("/gettable", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String content = "";
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));
    content = getMyDirAsString(LittleFS, "/", 0);
    //request->send(200, "text/html", content);
    request->send(200, "text/plain", content);
  });

  httpServer.on("/filesystem.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    String content = "";
    content = initialsetup_html;
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%crights%", htmlEscape(String(actconf.crights)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%cssid1%", htmlEscape(String(actconf.cssid1)));
    content.replace("%cpassword1%", "");
    content.replace("%cpassword1Masked%", maskSecret(String(actconf.cpassword1)));
    content.replace("%cssid2%", htmlEscape(String(actconf.cssid2)));
    content.replace("%cpassword2%", "");
    content.replace("%cpassword2Masked%", maskSecret(String(actconf.cpassword2)));
    content.replace("%cssid3%", htmlEscape(String(actconf.cssid3)));
    content.replace("%cpassword3%", "");
    content.replace("%cpassword3Masked%", maskSecret(String(actconf.cpassword3)));
    content.replace("%quality%", String(int(quality)));
    content.replace("%csrfToken%", getCsrfToken());

    content.replace("%wificonfig%", "");
    //content.replace("%tabelle%", getMyDirAsString(LittleFS, "/", 0));

    content.replace("%FREE_FILESYSTEM%", humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes())));
    content.replace("%USED_FILESYSTEM%", humanReadableSize(LittleFS.usedBytes()));
    content.replace("%TOTAL_FILESYSTEM%", humanReadableSize(LittleFS.totalBytes()));
    content.replace("%USED_FILESYSTEM_BYTES%", String(LittleFS.usedBytes()));
    content.replace("%TOTAL_FILESYSTEM_BYTES%", String(LittleFS.totalBytes()));

    request->send(200, "text/html", content);
  });

  httpServer.on("/sensorv.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String content = readFile2(LittleFS, "/sensorv.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%csrfToken%", getCsrfToken());

    String envSensorString = "";
    if (String(actconf.envSensor) == "BME280") {
    envSensorString +=F( "<h3>Environment <blink><data id='info'></data></blink>");
    envSensorString +=F( "</h3>");
    envSensorString +=F( "<FONT SIZE='4'>");
    envSensorString +=F( "<table>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
    envSensorString +=F( "<line x1='10' y1='9' x2='14' y2='9' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");

    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Temp:</td>");
    envSensorString +=F( "<td><data id='airtemp'></data><data id='atunit'></data></td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-wind' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M5 8h8.5a2.5 2.5 0 1 0 -2.34 -3.24' />");
    envSensorString +=F( "<path d='M3 12h15.5a2.5 2.5 0 1 1 -2.34 3.24' />");
    envSensorString +=F( "<path d='M4 16h5.5a2.5 2.5 0 1 1 -2.34 3.24' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Press:</td>");
    envSensorString +=F( "<td><data id='pressure'></data>mbar</td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-droplet' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M6.8 11a6 6 0 1 0 10.396 0l-5.197 -8l-5.2 8z' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Hum:</td>");
    envSensorString +=F( "<td><data id='humidity'></data>%</td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "<tr>");
    envSensorString +=F( "<td>");
    envSensorString +=F( "<div class='svg'>");
    envSensorString +=F( "<svg xmlns='http://www.w3.org/2000/svg' class='icon icon-tabler icon-tabler-temperature' width='30' height='30' viewBox='0 0 24 24' stroke-width='1.5' stroke='#FFFFFF' fill='none' stroke-linecap='round' stroke-linejoin='round'>");
    envSensorString +=F( "<path stroke='none' d='M0 0h24v24H0z' fill='none'/>");
    envSensorString +=F( "<path d='M10 13.5a4 4 0 1 0 4 0v-8.5a2 2 0 0 0 -4 0v8.5' />");
    envSensorString +=F( "<line x1='10' y1='9' x2='14' y2='9' />");
    envSensorString +=F( "</svg>");
    envSensorString +=F( "</div>");
    envSensorString +=F( "</td>");
    envSensorString +=F( "<td>Dew:</td>");
    envSensorString +=F( "<td><data id='dewpoint'></data><data id='dunit'></data></td>");
    envSensorString +=F( "<td></td>");
    envSensorString +=F( "</tr>");
    envSensorString +=F( "</table>");
    envSensorString +=F( "</FONT><br>");
    envSensorString +=F( "<hr align='left'>");
    }
    content.replace("%envSensorString%", String(envSensorString));

    //httpServer.sendHeader("Cache-Control", "max-age=600");
    //httpServer.send(200, "text/html", content);
    request->send(200, "text/html", content);
  });

  httpServer.on("/lora.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String content = readFile2(LittleFS, "/lora.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    request->send(200, "text/html", content);
  });

  httpServer.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String inputMessage;
    JsonDocument data;

    if (request->hasParam("data")) {
      int paramsNr = request->params();

      for(int i=0;i<paramsNr;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "data") {
          inputMessage = p->value();
          if (inputMessage == "alarm1") {
            DebugPrintln(3, "getdata param = alarm1");
            data["alarm1"] = alarm1;
          } if (inputMessage == "Tank1") {
            data["Tank1"] = String(tank1p);
            data["Tank1adc"] = String(tank1adc);
          }
        }
      }
      data["data"] = true;
    } else {
      data["ping"] = true;
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  });

  httpServer.on("/savesettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(405, "application/json", "{\"status\":\"error\",\"message\":\"Use POST for saving settings.\"}");
  });
  httpServer.on("/savesettings", HTTP_POST, handleSaveSettings);

  httpServer.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    request->send(LittleFS, "/settings.html", String(), false, settingsTemplateProcessor);
  });

  httpServer.on("/restart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send page
    String content = "";
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    content = readFile2(LittleFS, "/restart.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%csrfToken%", getCsrfToken());

    request->send(200, "text/html", content);
  });

  httpServer.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }

    DebugPrintln(3, "Restart requested via web interface");
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Device restart requested.\"}");
    reboot = true;
  });

  httpServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(405, "application/json", "{\"status\":\"error\",\"message\":\"Use POST for restart.\"}");
  });

  httpServer.on("/firmware.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check page password
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    String content = "";
    content = readFile2(LittleFS, "/firmware.html");

    String betaFirmwareUrl;
    String betaVersion;
    String betaError;
    const bool betaAvailable = resolveBetaFirmware(betaFirmwareUrl, betaVersion, &betaError);

    content.replace("%serverAvailable%", betaAvailable ? "Server available" : "Server not available");
    content.replace("%version2%", htmlEscape(betaAvailable ? betaVersion : "-"));
    content.replace("%betaFirmwareUrl%", htmlEscape(betaAvailable ? betaFirmwareUrl : ""));
    content.replace("%configuredFirmwareBaseUrl%", htmlEscape(getConfiguredFirmwareBaseUrl()));
    content.replace("%betaStatusDetail%", htmlEscape(betaAvailable ? "OK" : betaError));
    content.replace("%mdsOtaUrl%", htmlEscape(String(actconf.mdsOtaUrl)));
    content.replace("%mdsOtaSecretStatus%", strlen(actconf.mdsOtaSecret) > 0 ? "configured" : "not configured");

    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%csrfToken%", getCsrfToken());
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));

    request->send(200, "text/html", content);
  });

  httpServer.on("/startRemoteUpdate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      request->send(503, "application/json", buildOtaResponse("error", "No Wi-Fi connection available for remote update.", false, false, false));
      return;
    }

    if (remoteOtaPending || remoteOtaInProgress) {
      request->send(409, "application/json", buildOtaResponse("error", "A remote OTA update is already running.", false, false, false));
      return;
    }

    String source = request->hasParam("source", true) ? request->getParam("source", true)->value() : "";
    source.toLowerCase();

    String remoteUrl;
    String version;
    String sha256;
    String resolveError;
    bool resolved = false;
    if (source == "beta") {
      resolved = resolveBetaFirmware(remoteUrl, version, &resolveError, &sha256);
    } else if (source == "mds") {
      remoteUrl = String(actconf.mdsOtaUrl);
      remoteUrl.trim();
      version = "MDS";
      resolved = remoteUrl.startsWith("https://") && strlen(actconf.mdsOtaSecret) > 0;
      if (!resolved) {
        resolveError = "MDS OTA URL must use HTTPS and MDS OTA Secret must be configured.";
      }
    }

    if (!resolved || remoteUrl.length() == 0) {
      String message = "Unable to resolve remote firmware URL.";
      if (resolveError.length()) {
        message += " " + resolveError;
      }
      request->send(404, "application/json", buildOtaResponse("error", message, false, false, false));
      return;
    }

    const bool useMdsEndpoint = source == "mds";
    sha256 = normalizeSha256(sha256);
    if (!useMdsEndpoint && sha256.length() == 0) {
      request->send(422, "application/json", buildOtaResponse("error", "Remote firmware is missing a valid SHA256 checksum in firmware-manifest.json.", false, false, false));
      return;
    }

    remoteOtaUrl = remoteUrl;
    remoteOtaVersion = version;
    remoteOtaSha256 = sha256;
    remoteOtaUseMdsEndpoint = useMdsEndpoint;
    remoteOtaPending = true;
    startOtaProgress("queued", 0, "Remote firmware update queued on device...");

    const String message = "Remote firmware update queued" + (version.length() ? " (" + version + ")" : "") + ".";
    request->send(202, "application/json", buildOtaResponse("queued", message, false, false, true));
  });

  httpServer.on("/devinfo.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String content = readFile2(LittleFS, "/devinfo.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", htmlEscape(String(actconf.devname)));
    content.replace("%fversion%", String(actconf.fversion));
    content.replace("%license%", String(actconf.license));
    content.replace("%getSdkVersion%", String(ESP.getSdkVersion()));
    content.replace("%chipId%", String(chipId));
    content.replace("%getCpuFreqMHz%", String(String(ESP.getCpuFreqMHz())));
    content.replace("%hname%", String(hname));
    String mdnsname = "";
      if(actconf.mDNS == 1){
        mdnsname = String(hname) + ".local";
      }
      else{
        mdnsname =F( "not activ");
      }
    content.replace("%mdnsname%", mdnsname);
    content.replace("%sssid%", htmlEscape(String(actconf.sssid)));
    content.replace("%softAPIP%", WiFi.softAPIP().toString());
    content.replace("%WiFichannel%", String(WiFi.channel()));
    content.replace("%cssid1%", htmlEscape(String(actconf.cssid1)));
    content.replace("%cssid2%", htmlEscape(String(actconf.cssid2)));
    content.replace("%cssid3%", htmlEscape(String(actconf.cssid3)));
    content.replace("%localIP%", WiFi.localIP().toString());
    String mystring = String(actconf.devaddr, HEX);
    mystring.toUpperCase();
    content.replace("%devaddr%", mystring);

    String envSensorBME280 = "";
    if (String(actconf.envSensor) == "BME280") {
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Temperature</td>";
      envSensorBME280 += "<td><input id='airtemp' type='text' name='airtemp' size='15' value='0'></td>";
      envSensorBME280 += "<td>[<data id='atunit'></data>]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Pressure</td>";
      envSensorBME280 += "<td><input id='pressure' type='text' name='pressure' size='15' value='0'></td>";
      envSensorBME280 += "<td>[mbar]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Air Humidity</td>";
      envSensorBME280 += "<td><input id='humidity' type='text' name='humidity' size='15' value='0'></td>";
      envSensorBME280 += "<td>[%]</td>";
    envSensorBME280 += "</tr>";
    
    envSensorBME280 += "<tr>";
      envSensorBME280 += "<td>Dewpoint</td>";
      envSensorBME280 += "<td><input id='dewpoint' type='text' name='dewpoint' size='15' value='0'></td>";
      envSensorBME280 += "<td>[<data id='dpunit'></data>]</td>";
    envSensorBME280 += "</tr>";
  }
    content.replace("%envSensorBME280%", envSensorBME280);
    
    request->send(200, "text/html", content);
  });

  httpServer.serveStatic("/favicon.ico", LittleFS, "/favicon.ico").setCacheControl("max-age=600");

  httpServer.serveStatic("/settings.js", LittleFS, "/settings.js").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/common.css", LittleFS, "/common.css").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/common.js", LittleFS, "/common.js").setCacheControl("max-age=600");

  httpServer.serveStatic("/header.js", LittleFS, "/header.js").setCacheControl("max-age=600");

  httpServer.serveStatic("/restart.js", LittleFS, "/restart.js").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/index.js", LittleFS, "/index.js").setCacheControl("max-age=600");

  httpServer.on("/firmware_ota.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    String content = readFile2(LittleFS, "/firmware_ota.html");
    content.replace("%csrfToken%", getCsrfToken());
    request->send(200, "text/html", content);
  });

  httpServer.serveStatic("/firmware_ota.css", LittleFS, "/firmware_ota.css").setCacheControl("max-age=600");

  httpServer.serveStatic("/firmware_ota.js", LittleFS, "/firmware_ota.js").setCacheControl("max-age=600");

  httpServer.serveStatic("/firmware-page.js", LittleFS, "/firmware-page.js").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/sensorv.js", LittleFS, "/sensorv.js").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/lora.js", LittleFS, "/lora.js").setCacheControl("max-age=600");
  
  httpServer.serveStatic("/settings.js", LittleFS, "/settings.js").setCacheControl("max-age=600");

  httpServer.serveStatic("/settings-page.css", LittleFS, "/settings-page.css").setCacheControl("max-age=600");

  httpServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    long int t1 = millis();
    String content = "";
    String cssfilename = "";
    // Style activation
    switch (actconf.cssStyle) {
    case 0:
      cssfilename = "/css_black.css";
      break;
    case 1:
      cssfilename = "/css_red.css";
      break;
    case 2:
      cssfilename = "/css_white.css";
      break;  
    default:
      cssfilename = "/css_black.css";
    }
    if (LittleFS.exists(cssfilename)) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, cssfilename, "text/css");
      response->addHeader("Cache-Control", "max-age=100");
      request->send(response);
    } else {
      request->send(404, "text/plain", "The content you are looking for was not found.");
    }
    long int t2 = millis();
    DebugPrint(3, "Time taken by the task: ");
    DebugPrint(3, String(t2-t1));
    DebugPrintln(3, " milliseconds");
  });

  httpServer.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = readFile2(LittleFS, "/app.js");
    AsyncWebServerResponse *response = request->beginResponse(200, "text/javascript", content);
    response->addHeader("Cache-Control", "max-age=600");
    request->send(response);
  });

  httpServer.on("/staticdata.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    JsonDocument json_Device;
    json_Device["Device"]["Type"] = String(actconf.devname);
    json_Device["Device"]["CopyRights"] = String(actconf.crights);
    json_Device["Device"]["FirmwareVersion"] = String(actconf.fversion);
    json_Device["Device"]["License"] = String(actconf.license);

    json_Device["Device"]["ESP32"]["SDKVersion"] = String(ESP.getSdkVersion());
    json_Device["Device"]["ESP32"]["ChipID"] = String(chipId);
    json_Device["Device"]["ESP32"]["CPUSpeed"]["Value"] = String(ESP.getCpuFreqMHz());
    json_Device["Device"]["ESP32"]["CPUSpeed"]["Unit"] = "MHz";

    json_Device["Device"]["NetworkParameter"]["WLANClientSSID1"] = String(actconf.cssid1);
    json_Device["Device"]["NetworkParameter"]["WLANClientSSID2"] = String(actconf.cssid2);
    json_Device["Device"]["NetworkParameter"]["WLANClientSSID3"] = String(actconf.cssid3);
    json_Device["Device"]["NetworkParameter"]["WLANClientIP"] = WiFi.localIP().toString();

    json_Device["Device"]["NetworkParameter"]["WLANServerSSID"] = String(actconf.sssid);
    json_Device["Device"]["NetworkParameter"]["WLANServerIP"] = WiFi.softAPIP().toString();
    json_Device["Device"]["NetworkParameter"]["ServerMode"] = String(actconf.serverMode);
    json_Device["Device"]["NetworkParameter"]["ServerHostName"] = String(actconf.hostname);

    json_Device["Device"]["NetworkParameter"]["MdsUrl"] = String(actconf.MdsUrl);
    json_Device["Device"]["NetworkParameter"]["MdsApiKey"] = String(strlen(actconf.MdsApiKey) > 0 ? "***hidden***" : "");

    // Unused ?
    //json_Device["Device"]["DeviceSettings"]["SerialDebugMode"] = String(actconf.debug);
    //json_Device["Device"]["DeviceSettings"]["SerialSpeed"] = String(actconf.serspeed);
    json_Device["Device"]["DeviceSettings"]["WebSerialDebug"] = String(actconf.WebSerialDebug);
    //json_Device["Device"]["DeviceSettings"]["DeviceID"] = String(actconf.deviceID);
    //json_Device["Device"]["DeviceSettings"]["SendData"] = String(actconf.senddata);
    //json_Device["Device"]["DeviceSettings"]["VoltageOffset"] = String(actconf.voffset);
    //json_Device["Device"]["DeviceSettings"]["VoltageSlopeA1"] = String(actconf.a1vslope);
    //json_Device["Device"]["DeviceSettings"]["VoltageSlopeA2"] = String(actconf.a2vslope);
    //json_Device["Device"]["DeviceSettings"]["VoltageAverage"] = String(actconf.vaverage);
    //json_Device["Device"]["DeviceSettings"]["Tank1Offset"] = String(actconf.t1offset);
    //json_Device["Device"]["DeviceSettings"]["Tank1SlopeA1"] = String(actconf.a1t1slope);
    //json_Device["Device"]["DeviceSettings"]["Tank1SlopeA2"] = String(actconf.a2t1slope);
    //json_Device["Device"]["DeviceSettings"]["Tank1Average"] = String(actconf.t1average);
    //json_Device["Device"]["DeviceSettings"]["Tank2Offset"] = String(actconf.t2offset);
    //json_Device["Device"]["DeviceSettings"]["Tank2SlopeA1"] = String(actconf.a1t2slope);
    //json_Device["Device"]["DeviceSettings"]["Tank2SlopeA2"] = String(actconf.a2t2slope);
    //json_Device["Device"]["DeviceSettings"]["Tank2Average"] = String(actconf.t2average);
    //json_Device["Device"]["DeviceSettings"]["TempSensorType"] = String(actconf.tempSensorType);
    //json_Device["Device"]["DeviceSettings"]["TempUnit"] = String(actconf.tempUnit);
    json_Device["Device"]["DeviceSettings"]["envSensor"] = String(actconf.envSensor);
    json_Device["Device"]["DeviceSettings"]["standbyMode"] = String(actconf.standbyMode);

    String stringjsondata = "";
    serializeJson(json_Device, stringjsondata);

    request->send(200, "application/json", stringjsondata);
  });

  httpServer.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    //unsigned long previousMillis = 0;
    //unsigned long elapsedMillis = 0;
    //previousMillis = millis();
  
    JsonDocument json_Device;
    json_Device["Device"]["ESP32"]["FreeHeapSize"]["Value"] = String(ESP.getFreeHeap());
    json_Device["Device"]["ESP32"]["FreeHeapSize"]["Unit"] = "Byte";  // TODO: bring into staticdata.json

    json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Value"] = String(fieldstrength);
    json_Device["Device"]["NetworkParameter"]["FieldStrength"]["Unit"] = "dBm";  // TODO: bring into staticdata.json

    json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Value"] = String(int(quality));
    json_Device["Device"]["NetworkParameter"]["ConnectionQuality"]["Unit"] = "%";  // TODO: bring into staticdata.json

    String mydevaddr = String(actconf.devaddr, HEX);
    mydevaddr.toUpperCase();
    json_Device["Device"]["LoRaSettings"]["DeviceAddress"] = mydevaddr;  // TODO: bring into staticdata.json
    json_Device["Device"]["LoRaSettings"]["Frequency"] = String(actconf.lorafrequency);
    json_Device["Device"]["LoRaSettings"]["Channel"] = String(actconf.lchannel);
    json_Device["Device"]["LoRaSettings"]["ActualChannel"] = String(getLMICtxChnl());
    json_Device["Device"]["LoRaSettings"]["SpreadingFactor"] = String(actconf.spreadf);
    json_Device["Device"]["LoRaSettings"]["ActualSF"] = String(sf);
    json_Device["Device"]["LoRaSettings"]["DynamicSF"] = String(actconf.dynsf);
    json_Device["Device"]["LoRaSettings"]["TXInterval"] = String(actconf.tinterval);
    json_Device["Device"]["LoRaSettings"]["TimeSlot"] = String(slot);
    json_Device["Device"]["LoRaSettings"]["TXCounter"] = String(getLMICseqnoUp());
    json_Device["Device"]["LoRaSettings"]["Relay"] = String(actconf.relay);

    json_Device["Device"]["DisplaySettings"]["Skin"] = String(actconf.skin);
    json_Device["Device"]["DisplaySettings"]["InstrumentSize"] = String(actconf.instrumentSize);

    json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Value"] = String(temperature, 1);
    json_Device["Device"]["MeasuringValues"]["AirTemperature"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["AirPressure"]["Value"] = String(pressure, 0);
    json_Device["Device"]["MeasuringValues"]["AirPressure"]["Unit"] = "mbar";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Value"] = String(humidity, 0);
    json_Device["Device"]["MeasuringValues"]["AirHumidity"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Value"] = String(dewp, 1);
    json_Device["Device"]["MeasuringValues"]["Dewpoint"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Value"] = String(temp1wire, 1);
    json_Device["Device"]["MeasuringValues"]["Temp1Wire"]["Unit"] = String(actconf.tempUnit);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Value"] = String(voltage, 3);
    json_Device["Device"]["MeasuringValues"]["BatteryVoltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Value"] = String(capacity, 0);
    json_Device["Device"]["MeasuringValues"]["BatteryCapacity"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Value"] = String(tank1, 3);
    json_Device["Device"]["MeasuringValues"]["Tank1Voltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Value"] = String(tank2, 3);
    json_Device["Device"]["MeasuringValues"]["Tank2Voltage"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1"]["Value"] = String(tank1p, 3);
    json_Device["Device"]["MeasuringValues"]["Tank1"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank1adc"]["Value"] = String(tank1adc);

    json_Device["Device"]["MeasuringValues"]["Tank2"]["Value"] = String(tank2p, 3);
    json_Device["Device"]["MeasuringValues"]["Tank2"]["Unit"] = "%";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Tank2adc"]["Value"] = String(tank2adc);

    json_Device["Device"]["MeasuringValues"]["Alarm"]["Value"] = String(alarm1);
    json_Device["Device"]["MeasuringValues"]["Alarm"]["Unit"] = "bin";  // TODO: bring into staticdata.json

    const int standbyInputRawLevel = digitalRead(alarmPin);
    json_Device["Device"]["MeasuringValues"]["StandbyInputPin"]["Value"] = String(alarmPin);
    json_Device["Device"]["MeasuringValues"]["StandbyInputPin"]["Unit"] = "GPIO";
    json_Device["Device"]["MeasuringValues"]["StandbyInputLevel"]["Value"] = standbyInputRawLevel == LOW ? "LOW" : "HIGH";
    json_Device["Device"]["MeasuringValues"]["StandbyInputLevel"]["Unit"] = "active LOW";
    json_Device["Device"]["MeasuringValues"]["StandbyInputState"]["Value"] = alarm1 ? "Active" : "Inactive";
    json_Device["Device"]["MeasuringValues"]["StandbyInputState"]["Unit"] = alarm1 ? "awake/active" : "standby/sleep";

    json_Device["Device"]["MeasuringValues"]["Relay"]["Value"] = String(actconf.relay);
    json_Device["Device"]["MeasuringValues"]["Relay"]["Unit"] = "bin";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Value"] = String(int(relaytimer * 5));
    json_Device["Device"]["MeasuringValues"]["RelayTimer"]["Unit"] = "x5min";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Value"] = String(actconf.envSensor);
    json_Device["Device"]["MeasuringValues"]["EnvSensor"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["standbyMode"]["Value"] = String(actconf.standbyMode);
    json_Device["Device"]["MeasuringValues"]["standbyMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["loraOperationMode"]["Value"] = String(actconf.loraOperationMode);
    json_Device["Device"]["MeasuringValues"]["loraOperationMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["WifiStandbyMode"]["Value"] = String(actconf.WifiStandbyMode);
    json_Device["Device"]["MeasuringValues"]["WifiStandbyMode"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["SendDataViaWifi"]["Value"] = String(actconf.SendDataViaWifi);
    json_Device["Device"]["MeasuringValues"]["SendDataViaWifi"]["Unit"] = "";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Value"] = String(vedirectVoltage, 3);
    json_Device["Device"]["MeasuringValues"]["VEdirectV"]["Unit"] = "V";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Value"] = String(vedirectCurrent, 3);
    json_Device["Device"]["MeasuringValues"]["VEdirectC"]["Unit"] = "A";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Value"] = String(vedirectTemp, 1);
    json_Device["Device"]["MeasuringValues"]["VEdirectT"]["Unit"] = "°C";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Latitude"]["Value"] = String(latitude, 6);
    json_Device["Device"]["MeasuringValues"]["Latitude"]["Unit"] = String(latitudeNS);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Longitude"]["Value"] = String(longitude, 6);
    json_Device["Device"]["MeasuringValues"]["Longitude"]["Unit"] = String(longitudeEW);  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Altitude"]["Value"] = String(altitude, 0);
    json_Device["Device"]["MeasuringValues"]["Altitude"]["Unit"] = "m";  // TODO: bring into staticdata.json

    String zhour = firstzero(hour);
    String zminute = firstzero(minute);
    String zsecond = firstzero(second); 
    String timestr = String(zhour) + ":" + String(zminute) + ":" + String(zsecond);
    json_Device["Device"]["MeasuringValues"]["Time"]["Value"] = String(timestr);
    json_Device["Device"]["MeasuringValues"]["Time"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    String zday = firstzero(day);
    String zmonth = firstzero(month);
    String zyear = firstzero(year);
    String datestr = String(zday) + "." + String(zmonth) + "." + String(zyear);
    json_Device["Device"]["MeasuringValues"]["Date"]["Value"] = String(datestr);
    json_Device["Device"]["MeasuringValues"]["Date"]["Unit"] = "GMT";  // TODO: bring into staticdata.json

    String sunrisestr = String(zhour) + ":" + String(zminute) + ":" + String(zsecond);
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Value"] = String(sunrisestr);
    json_Device["Device"]["MeasuringValues"]["Sunrise"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    String sunsetstr = String(zhour) + ":" + String(zminute) + ":" + String(zsecond);
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Value"] = String(sunsetstr);
    json_Device["Device"]["MeasuringValues"]["Sunset"]["Unit"] = "UTC";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Speed"]["Value"] = String(gpsspeed);
    json_Device["Device"]["MeasuringValues"]["Speed"]["Unit"] = "kn";  // TODO: bring into staticdata.json

    json_Device["Device"]["MeasuringValues"]["Course"]["Value"] = String(course);
    json_Device["Device"]["MeasuringValues"]["Course"]["Unit"] = "°";  // TODO: bring into staticdata.json

    // for debugging purpose?
    //json_Device["Device"]["NMEAValues"]["String1"] = sendXDR1(0);
    //json_Device["Device"]["NMEAValues"]["String2"] = sendXDR2(0);
    //json_Device["Device"]["NMEAValues"]["String3"] = sendXDR3(0);
    //json_Device["Device"]["NMEAValues"]["String4"] = "°";
    //json_Device["Device"]["NMEAValues"]["String5"] = "°";

    String stringjsondata = "";
    serializeJson(json_Device, stringjsondata);

    request->send(200, "application/json", stringjsondata);

    //elapsedMillis = millis() - previousMillis;
    //DebugPrint(3, "Wert Stoppuhr: " + String(elapsedMillis));
  });

  httpServer.on("/reseteeprom", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    actconf = defconf;
    saveEEPROMConfig(defconf);
    request->send(200, "text/javascript", "ok, EEPROM erased.");
    ESP.restart();
  });

  httpServer.on("/restoreconfigbackup", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }

    configData restoredConfig;
    if (!restoreConfigBackupFromLittleFS(restoredConfig)) {
      request->send(404, "application/json", buildOtaResponse("error", "No valid config backup found in LittleFS.", false, false, false));
      return;
    }

    String currentVersion = String(defconf.fversion);
    currentVersion.toCharArray(restoredConfig.fversion, sizeof(restoredConfig.fversion));
    restoredConfig.valid = defconf.valid;
    if (restoredConfig.password[0] == '\0') {
      strncpy(restoredConfig.password, actconf.password, sizeof(restoredConfig.password) - 1);
      restoredConfig.password[sizeof(restoredConfig.password) - 1] = '\0';
    }
    if (restoredConfig.cpassword1[0] == '\0') {
      strncpy(restoredConfig.cpassword1, actconf.cpassword1, sizeof(restoredConfig.cpassword1) - 1);
      restoredConfig.cpassword1[sizeof(restoredConfig.cpassword1) - 1] = '\0';
    }
    if (restoredConfig.cpassword2[0] == '\0') {
      strncpy(restoredConfig.cpassword2, actconf.cpassword2, sizeof(restoredConfig.cpassword2) - 1);
      restoredConfig.cpassword2[sizeof(restoredConfig.cpassword2) - 1] = '\0';
    }
    if (restoredConfig.cpassword3[0] == '\0') {
      strncpy(restoredConfig.cpassword3, actconf.cpassword3, sizeof(restoredConfig.cpassword3) - 1);
      restoredConfig.cpassword3[sizeof(restoredConfig.cpassword3) - 1] = '\0';
    }
    if (restoredConfig.spassword[0] == '\0') {
      strncpy(restoredConfig.spassword, actconf.spassword, sizeof(restoredConfig.spassword) - 1);
      restoredConfig.spassword[sizeof(restoredConfig.spassword) - 1] = '\0';
    }
    if (restoredConfig.MdsApiKey[0] == '\0') {
      strncpy(restoredConfig.MdsApiKey, actconf.MdsApiKey, sizeof(restoredConfig.MdsApiKey) - 1);
      restoredConfig.MdsApiKey[sizeof(restoredConfig.MdsApiKey) - 1] = '\0';
    }
    if (restoredConfig.mdsOtaSecret[0] == '\0') {
      strncpy(restoredConfig.mdsOtaSecret, actconf.mdsOtaSecret, sizeof(restoredConfig.mdsOtaSecret) - 1);
      restoredConfig.mdsOtaSecret[sizeof(restoredConfig.mdsOtaSecret) - 1] = '\0';
    }
    bool hasRestoredNskey = false;
    bool hasRestoredAppkey = false;
    for (size_t i = 0; i < sizeof(restoredConfig.nskey); i++) {
      hasRestoredNskey = hasRestoredNskey || restoredConfig.nskey[i] != 0;
      hasRestoredAppkey = hasRestoredAppkey || restoredConfig.appkey[i] != 0;
    }
    if (!hasRestoredNskey) {
      memcpy(restoredConfig.nskey, actconf.nskey, sizeof(restoredConfig.nskey));
    }
    if (!hasRestoredAppkey) {
      memcpy(restoredConfig.appkey, actconf.appkey, sizeof(restoredConfig.appkey));
    }
    actconf = restoredConfig;
    saveEEPROMConfig(actconf);

    request->send(200, "application/json", buildOtaResponse("ok", "Config backup restored. Device reboots now.", true, false, true));
    delay(250);
    ESP.restart();
  });

  httpServer.serveStatic("/gauge.min.js", LittleFS, "/gauge.min.js").setCacheControl("max-age=600");

  auto handleOtaPost = [](AsyncWebServerRequest *request) {
      if (actconf.crypt == 1) {
        if(!request->authenticate(actconf.username, actconf.password)) {
          return request->requestAuthentication();
        }
      }
      if (!requireCsrfToken(request)) {
        return;
      }
      if (!requireNonDefaultWebPassword(request)) {
        return;
      }
    };

  auto handleOtaUpload = [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {
                    if (actconf.crypt == 1) {
                      if(!request->authenticate(actconf.username, actconf.password)) {
                        return;
                      }
                    }
                    if (!isCsrfTokenValid(request)) {
                      if (index == 0) {
                        finishOtaProgress(false, "Invalid CSRF token.");
                      }
                      return;
                    }
                    if (isDefaultWebPasswordActive()) {
                      if (index == 0) {
                        finishOtaProgress(false, "Default web password must be changed before firmware updates.");
                      }
                      return;
                    }
                    handleDoUpdate(request, filename, index, data, len, final);
                  };

  httpServer.on("/doUpdate", HTTP_POST,
    handleOtaPost,
    handleOtaUpload
  );

  #ifdef ESP32
    //Update.onProgress(printProgress);
  #endif

  // run handleUpload function when any file is uploaded
  httpServer.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (actconf.crypt == 1) {
        if(!request->authenticate(actconf.username, actconf.password)) {
          return request->requestAuthentication();
        }
      }
      if (!requireCsrfToken(request)) {
        return;
      }
      if (!requireNonDefaultWebPassword(request)) {
        return;
      }
      request->send(200);
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {
                    if (actconf.crypt == 1) {
                      if(!request->authenticate(actconf.username, actconf.password)) {
                        return;
                      }
                    }
                    if (!isCsrfTokenValid(request)) {
                      return;
                    }
                    if (isDefaultWebPasswordActive()) {
                      return;
                    }
                    handleUpload(request, filename, index, data, len, final);
                  }
  );

  /*handling uploading file */
  /*httpServer.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  //httpServer.on("/upload", HTTP_POST, [](){
    httpServer.sendHeader("Connection", "close");
  },[](){  
    HTTPUpload& upload = httpServer.upload();
      root = LittleFS.open((String("/") + upload.filename).c_str(), FILE_WRITE);
      if(!root){
        DebugPrintln(1, "- failed to open file for writing");
        return;
      }
    if(upload.status == UPLOAD_FILE_WRITE){
      if(root.write(upload.buf, upload.currentSize) != upload.currentSize){
        DebugPrintln(1, "- failed to write");
        return;
      }
    } else if(upload.status == UPLOAD_FILE_END){
      root.close();
      DebugPrintln(3, "UPLOAD_FILE_END");
    }
  });*/

  httpServer.on("/formatfs", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    formatfs(LittleFS);
    request->send(200, "text/html", "done");
  });

  httpServer.on("/updatefiles", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    runDownloadingFiles = true;
    request->send(200, "text/html", "done");
  });

  httpServer.on("/updatefilesstatus", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    String test = String(runDownloadingFilesStatus || runDownloadingFiles);
    request->send(200, "text/html", test);
  });

  httpServer.on("/updatefilesprogress", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    request->send(200, "application/json", buildUpdateFilesProgressJson());
  });

  httpServer.on("/updatefilesinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    const String firmwareVersion = String(actconf.fversion);
    const String storedWebFilesVersion = getStoredWebFilesVersion();
    const bool busy = runDownloadingFilesStatus || runDownloadingFiles;
    const bool upToDate = areWebFilesCurrent(actconf.fversion);

    String response = "{\"busy\":";
    response += busy ? "true" : "false";
    response += ",\"firmwareVersion\":\"";
    response += firmwareVersion;
    response += "\",\"storedWebFilesVersion\":\"";
    response += storedWebFilesVersion;
    response += "\",\"upToDate\":";
    response += upToDate ? "true" : "false";
    response += "}";

    request->send(200, "application/json", response);
  });

  httpServer.on("/otaprogress", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    request->send(200, "application/json", buildOtaProgressJson());
  });

  httpServer.on("/mdsotainfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }

    JsonDocument response;
    fetchMdsOtaInfo(response);
    String body;
    serializeJson(response, body);
    request->send(200, "application/json", body);
  });

  httpServer.on("/testMdsUpload", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1) {
      if(!request->authenticate(actconf.username, actconf.password)) {
        return request->requestAuthentication();
      }
    }
    if (!requireCsrfToken(request)) {
      return;
    }
    if (!requireNonDefaultWebPassword(request)) {
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      request->send(503, "application/json", "{\"status\":\"error\",\"message\":\"No Wi-Fi connection available for MDS test.\"}");
      return;
    }

    DebugPrintln(3, "Manual Test MDS Upload triggered");
    const bool success = sendToMDS(actconf);
    JsonDocument response;
    if (success) {
      response["status"] = "ok";
      response["message"] = "MDS test upload sent successfully.";
      response["detail"] = getLastMdsStatus();
      String body;
      serializeJson(response, body);
      request->send(200, "application/json", body);
    } else {
      response["status"] = "error";
      response["message"] = "MDS test upload failed.";
      response["detail"] = getLastMdsStatus();
      String body;
      serializeJson(response, body);
      request->send(502, "application/json", body);
    }
  });

  httpServer.onNotFound([](AsyncWebServerRequest *request){
    if (request->method() == HTTP_OPTIONS) 
    {
      request->send(200);
    } 
    else 
    {
      String content = readFile2(LittleFS, "/error.html");
      content.replace("%header%", getheader(actconf));
      content.replace("%devname%", htmlEscape(String(actconf.devname)));
      content.replace("%path%", htmlEscape(request->url()));
      request->send(404, "text/html", content);
    }
  });

  httpServer.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  httpServer.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", logout_html2);
  });
    
}

#ifdef ESP8266
#include <Updater.h>
#include <ESP8266mDNS.h>
#define U_PART U_FS
#else
#include <Update.h>
#include <ESPmDNS.h>
#define U_PART U_SPIFFS
#endif

size_t content_len;

namespace {
bool otaProgressActive = false;
bool otaProgressSuccess = false;
size_t otaProgressCurrent = 0;
size_t otaProgressTotal = 0;
String otaProgressPhase = "idle";
String otaProgressMessage = "";

void resetOtaProgress() {
  otaProgressActive = false;
  otaProgressSuccess = false;
  otaProgressCurrent = 0;
  otaProgressTotal = 0;
  otaProgressPhase = "idle";
  otaProgressMessage = "";
}

void startOtaProgress(const String &phase, size_t totalBytes, const String &message) {
  otaProgressActive = true;
  otaProgressSuccess = false;
  otaProgressCurrent = 0;
  otaProgressTotal = totalBytes;
  otaProgressPhase = phase;
  otaProgressMessage = message;
}

void updateOtaProgress(const String &phase, size_t currentBytes, size_t totalBytes, const String &message) {
  otaProgressActive = true;
  otaProgressCurrent = currentBytes;
  if (totalBytes > 0) {
    otaProgressTotal = totalBytes;
  }
  otaProgressPhase = phase;
  otaProgressMessage = message;
}

void finishOtaProgress(bool success, const String &message) {
  otaProgressActive = false;
  otaProgressSuccess = success;
  if (otaProgressTotal > 0) {
    otaProgressCurrent = otaProgressTotal;
  }
  otaProgressPhase = success ? "complete" : "error";
  otaProgressMessage = message;
}

String buildOtaProgressJson() {
  JsonDocument json;
  json["active"] = otaProgressActive;
  json["success"] = otaProgressSuccess;
  json["phase"] = otaProgressPhase;
  json["message"] = otaProgressMessage;
  json["current"] = otaProgressCurrent;
  json["total"] = otaProgressTotal;
  json["percent"] = otaProgressTotal > 0 ? (otaProgressCurrent * 100) / otaProgressTotal : 0;

  String response;
  serializeJson(json, response);
  return response;
}

String buildUpdateFilesProgressJson() {
  JsonDocument json;
  const bool busy = runDownloadingFilesStatus || runDownloadingFiles;
  json["busy"] = busy;
  json["firmwareVersion"] = String(actconf.fversion);
  json["storedWebFilesVersion"] = getStoredWebFilesVersion();
  json["upToDate"] = areWebFilesCurrent(actconf.fversion);
  json["completed"] = webFilesDownloadCompleted;
  json["total"] = webFilesDownloadTotal;
  json["currentFile"] = webFilesDownloadCurrentName;
  json["percent"] = webFilesDownloadTotal > 0 ? (webFilesDownloadCompleted * 100) / webFilesDownloadTotal : 0;

  String response;
  serializeJson(json, response);
  return response;
}
}

static bool isFilesystemUpdateRequest(AsyncWebServerRequest *request, const String& filename) {
  String loweredFilename = filename;
  loweredFilename.toLowerCase();

  if (request != nullptr) {
    if (request->hasParam("filesystem", true, true) || request->hasParam("filesystem", true)) {
      return true;
    }
  }

  return loweredFilename.indexOf("littlefs") > -1 ||
         loweredFilename.indexOf("filesystem") > -1;
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage) {
  return performRemoteOtaUpdate(url, filesystemUpdate, errorMessage, "", false);
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256) {
  return performRemoteOtaUpdate(url, filesystemUpdate, errorMessage, expectedSha256, false);
}

bool performRemoteOtaUpdate(const String &url, bool filesystemUpdate, String &errorMessage, const String &expectedSha256, bool useMdsOtaEndpoint) {
  if (!url.startsWith("https://")) {
    errorMessage = "Remote update URLs must use HTTPS.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  String normalizedExpectedSha256 = normalizeSha256(expectedSha256);
  if (!filesystemUpdate && expectedSha256.length() > 0 && normalizedExpectedSha256.length() == 0) {
    errorMessage = "Remote firmware checksum is invalid.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  if (!filesystemUpdate && !saveConfigBackupToLittleFS(actconf)) {
    errorMessage = "Config backup failed. Firmware update cancelled.";
    finishOtaProgress(false, errorMessage);
    return false;
  }

  WiFiClientSecure client;
  HTTPClient http;
  client.setCACert(reinterpret_cast<const char*>(cert_cacert_pem_start));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (useMdsOtaEndpoint) {
    const char *responseHeaders[] = { "x-SHA256", "X-SHA256", "x-MD5", "X-MD5" };
    http.collectHeaders(responseHeaders, 4);
    http.setUserAgent("ESP32-http-Update");
  }

  if (!http.begin(client, url)) {
    errorMessage = "Unable to initialize remote HTTPS update request.";
    finishOtaProgress(false, errorMessage);
    return false;
  }
  if (useMdsOtaEndpoint) {
    http.addHeader("X-MDS-OTA-Secret", String(actconf.mdsOtaSecret));
    http.addHeader("x-ESP32-STA-MAC", WiFi.macAddress());
    http.addHeader("x-ESP32-sketch-md5", ESP.getSketchMD5());
    http.addHeader("x-ESP32-sdk-version", String(ESP.getSdkVersion()));
    http.addHeader("x-ESP32-version", String(actconf.fversion));
  }

  startOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware", 0,
                   filesystemUpdate ? "Downloading file system update from server..." : "Downloading firmware from server...");
  const int httpCode = http.GET();
  if (useMdsOtaEndpoint && httpCode == HTTP_CODE_NOT_MODIFIED) {
    otaProgressActive = false;
    otaProgressSuccess = true;
    otaProgressCurrent = 0;
    otaProgressTotal = 0;
    otaProgressPhase = "no-update";
    otaProgressMessage = "MDS reports no newer firmware available.";
    http.end();
    return true;
  }
  if (httpCode != HTTP_CODE_OK) {
    errorMessage = "Remote update download failed: HTTP " + String(httpCode);
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  const int cmd = filesystemUpdate ? U_PART : U_FLASH;
  const int contentLength = http.getSize();
  String expectedMd5 = "";
  if (useMdsOtaEndpoint) {
    normalizedExpectedSha256 = normalizeSha256(http.header("x-SHA256"));
    if (normalizedExpectedSha256.length() == 0) {
      normalizedExpectedSha256 = normalizeSha256(http.header("X-SHA256"));
    }
    if (normalizedExpectedSha256.length() == 0) {
      errorMessage = "MDS OTA response is missing a valid SHA256 checksum.";
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }

    expectedMd5 = http.header("x-MD5");
    if (expectedMd5.length() == 0) {
      expectedMd5 = http.header("X-MD5");
    }
    expectedMd5.trim();
  }
  if (contentLength > 0) {
    startOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware",
                     static_cast<size_t>(contentLength),
                     filesystemUpdate ? "Downloading file system update from server..." : "Downloading firmware from server...");
  }
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
    Update.printError(Serial);
    errorMessage = "Unable to start OTA update.";
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }
  if (useMdsOtaEndpoint && expectedMd5.length() == 32) {
    Update.setMD5(expectedMd5.c_str());
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  unsigned long lastReadMs = millis();
  mbedtls_sha256_context shaContext;
  const bool verifySha256 = !filesystemUpdate && normalizedExpectedSha256.length() == 64;
  if (verifySha256) {
    mbedtls_sha256_init(&shaContext);
    mbedtls_sha256_starts_ret(&shaContext, 0);
  }
  while (http.connected() && (contentLength <= 0 || static_cast<int>(written) < contentLength)) {
    const size_t availableBytes = stream->available();
    if (availableBytes == 0) {
      if (!http.connected() && !stream->available()) {
        break;
      }
      if (millis() - lastReadMs > 15000) {
        break;
      }
      delay(1);
      continue;
    }

    const size_t toRead = availableBytes > sizeof(buffer) ? sizeof(buffer) : availableBytes;
    const size_t bytesRead = stream->readBytes(buffer, toRead);
    if (bytesRead == 0) {
      continue;
    }

    lastReadMs = millis();
    if (Update.write(buffer, bytesRead) != bytesRead) {
      Update.printError(Serial);
      errorMessage = "Downloaded firmware could not be written completely.";
      Update.abort();
      if (verifySha256) {
        mbedtls_sha256_free(&shaContext);
      }
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }

    if (verifySha256) {
      mbedtls_sha256_update_ret(&shaContext, buffer, bytesRead);
    }
    written += bytesRead;
    updateOtaProgress(filesystemUpdate ? "download-filesystem" : "download-firmware",
                      written,
                      contentLength > 0 ? static_cast<size_t>(contentLength) : written,
                      filesystemUpdate ? "Downloading and writing file system update..." : "Downloading and writing firmware...");
  }

  if (written == 0 || (contentLength > 0 && static_cast<int>(written) != contentLength)) {
    Update.printError(Serial);
    errorMessage = "Downloaded firmware could not be written completely.";
    Update.abort();
    if (verifySha256) {
      mbedtls_sha256_free(&shaContext);
    }
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  if (verifySha256) {
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&shaContext, digest);
    mbedtls_sha256_free(&shaContext);

    const String actualSha256 = sha256ToHex(digest);
    if (actualSha256 != normalizedExpectedSha256) {
      errorMessage = "Remote firmware checksum mismatch.";
      Update.abort();
      finishOtaProgress(false, errorMessage);
      http.end();
      return false;
    }
  }

  updateOtaProgress("finalizing", written, written, "Finalizing update...");
  if (!Update.end(true)) {
    Update.printError(Serial);
    errorMessage = "Update finalize failed.";
    finishOtaProgress(false, errorMessage);
    http.end();
    return false;
  }

  if (filesystemUpdate) {
    saveWebFilesVersion(actconf.fversion);
  }

  http.end();
  remoteOtaRebootRequired = true;
  finishOtaProgress(true, filesystemUpdate ? "File system update complete. Device reboots now." : "Firmware update complete. Device reboots now.");
  return true;
}

// TODO create new function for downloading beta file and run this funciton (from beta)
void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  const bool filesystemUpdate = isFilesystemUpdateRequest(request, filename);

  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    localOtaInProgress = true;
    standbySleepBlockedUntilMillis = millis() + 180000UL;
    startOtaProgress(filesystemUpdate ? "upload-filesystem" : "upload-firmware", content_len,
                     filesystemUpdate ? "Uploading file system update..." : "Uploading firmware...");
    // Detect filesystem uploads both from modern field names and legacy filenames.
    int cmd = filesystemUpdate ? U_PART : U_FLASH;
    if (!filesystemUpdate && !saveConfigBackupToLittleFS(actconf)) {
      localOtaInProgress = false;
      finishOtaProgress(false, "Config backup failed. Firmware update cancelled.");
      request->send(500, "application/json", buildOtaResponse("error", "Config backup failed. Firmware update cancelled.", false, false, false));
      return;
    }
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
      localOtaInProgress = false;
      finishOtaProgress(false, "Unable to start OTA update.");
      request->send(500, "application/json", buildOtaResponse("error", "Unable to start OTA update.", false, false, !filesystemUpdate));
      return;
    }
  }

  const size_t written = Update.write(data, len);
  if (written != len) {
    Update.printError(Serial);
    Update.abort();
    localOtaInProgress = false;
    finishOtaProgress(false, "Update write failed.");
    request->send(500, "application/json", buildOtaResponse("error", "Update write failed.", false, false, !filesystemUpdate));
    return;
  }
#ifdef ESP8266
  if (Update.size() > 0) {
    Serial.printf("Progress: %d%%\n", (Update.progress() * 100) / Update.size());
  }
#endif
  updateOtaProgress(filesystemUpdate ? "upload-filesystem" : "upload-firmware",
                    index + len,
                    content_len,
                    filesystemUpdate ? "Uploading file system update..." : "Uploading firmware...");
  //Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
  if (final) {
    updateOtaProgress("finalizing", content_len, content_len, "Finalizing update...");
    if (!Update.end(true)){
      Update.printError(Serial);
      localOtaInProgress = false;
      finishOtaProgress(false, "Update failed. See serial log for details.");
      request->send(500, "application/json", buildOtaResponse("error", "Update failed. See serial log for details.", false, false, !filesystemUpdate));
    } else {
      if (filesystemUpdate) {
        saveWebFilesVersion(actconf.fversion);
      }
      Serial.println("Update complete");
      const String message = filesystemUpdate
        ? "FileSystem update complete. Device reboots now."
        : "Firmware update complete. Device reboots now.";
      finishOtaProgress(true, message);
      request->send(200, "application/json", buildOtaResponse("ok", message, true, false, !filesystemUpdate));
      localOtaInProgress = false;
      scheduledRestartMillis = millis() + 2000UL;
    }
  }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}
