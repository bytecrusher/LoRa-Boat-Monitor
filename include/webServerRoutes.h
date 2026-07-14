#ifndef WEB_SERVER_ROUTES_H
#define WEB_SERVER_ROUTES_H

#include <ESPAsyncWebServer.h>

void registerPageRoutes();
void registerStaticAssetRoutes();
void registerApiRoutes();
void registerMaintenanceRoutes();
void registerFallbackRoutes();

bool requireAuthenticatedRequest(AsyncWebServerRequest *request);
void noteWebApiActivity();
void notePageRequestActivity();
bool requireStrongPagePassword(AsyncWebServerRequest *request);
String escapePageHtml(const String &value);
String renderInitialSetupPage(const String &baseContent);
String renderEnvironmentSensorMarkup();
String processSettingsPageTemplate(const String &variable);
uint16_t samplePageBatteryAdcRaw(size_t samples);
void handleSettingsPageSave(AsyncWebServerRequest *request);
String createPageOtaResponse(const char *status, const String &message, bool rebooting, bool checkWebFiles, bool backupSaved);
bool queuePageRemoteOtaUpdate(const String &source, bool forceReinstall, String &errorMessage);

#endif
