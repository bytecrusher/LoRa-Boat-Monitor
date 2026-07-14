#include "func_webServerHandler.h"
#include "webServerRoutes.h"

namespace {
void sendLittleFsAsset(AsyncWebServerRequest *request,
                       const char *path,
                       const char *contentType,
                       const char *cacheControl = "private, max-age=60, must-revalidate")
{
  if (!LittleFS.exists(path)) {
    request->send(404, "text/plain", "The requested web interface file was not found.");
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
  response->addHeader("Cache-Control", cacheControl);
  request->send(response);
}
}

void registerStaticAssetRoutes()
{
  httpServer.serveStatic("/favicon.ico", LittleFS, "/favicon.ico").setCacheControl("max-age=600");

  httpServer.on("/settings.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/settings.js", "text/javascript");
  });
  httpServer.on("/common.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/common.css", "text/css");
  });
  httpServer.on("/common.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/common.js", "text/javascript");
  });
  httpServer.on("/header.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/header.js", "text/javascript");
  });
  httpServer.on("/restart.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/restart.js", "text/javascript");
  });
  httpServer.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/index.js", "text/javascript");
  });
  httpServer.on("/firmware-page.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/firmware-page.js", "text/javascript");
  });
  httpServer.on("/filesystem.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/filesystem.js", "text/javascript");
  });
  httpServer.on("/sensorv.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/sensorv.js", "text/javascript");
  });
  httpServer.on("/lora.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/lora.js", "text/javascript");
  });
  httpServer.on("/settings-page.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/settings-page.css", "text/css");
  });

  httpServer.on("/firmware_ota.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1 && !request->authenticate(actconf.username, actconf.password)) {
      return request->requestAuthentication();
    }
    request->redirect("/firmware.html");
  });
  httpServer.on("/firmware_ota.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Legacy firmware OTA stylesheet is no longer used.");
  });
  httpServer.on("/firmware_ota.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Legacy firmware OTA script is no longer used.");
  });

  httpServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    const char *cssPath = "/css_black.css";
    if (actconf.cssStyle == 1) {
      cssPath = "/css_red.css";
    } else if (actconf.cssStyle == 2) {
      cssPath = "/css_white.css";
    }
    sendLittleFsAsset(request, cssPath, "text/css", "no-store, max-age=0");
  });

  httpServer.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    sendLittleFsAsset(request, "/app.js", "text/javascript");
  });
}
