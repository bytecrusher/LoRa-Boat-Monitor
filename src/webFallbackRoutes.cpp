#include "func_webServerHandler.h"
#include "webServerRoutes.h"

namespace {
const char LOGGED_OUT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
  <head><meta name="viewport" content="width=device-width, initial-scale=1"></head>
  <body>
    <p>Logged out or <a href="/">return to homepage</a>.</p>
    <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
  </body>
</html>
)rawliteral";

String escapeHtml(const String &value)
{
  String escaped;
  escaped.reserve(value.length());
  for (size_t index = 0; index < value.length(); ++index) {
    switch (value[index]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[index]; break;
    }
  }
  return escaped;
}
}

void registerFallbackRoutes()
{
  httpServer.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
      return;
    }

    String content = readFile2(LittleFS, "/error.html");
    content.replace("%header%", getheader(actconf));
    content.replace("%devname%", escapeHtml(String(actconf.devname)));
    content.replace("%path%", escapeHtml(request->url()));
    request->send(404, "text/html", content);
  });

  httpServer.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(401);
  });
  httpServer.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", LOGGED_OUT_HTML);
  });
}
