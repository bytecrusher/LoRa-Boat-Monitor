#include "func_webServerHandler.h"
#include "webServerRoutes.h"

void WebServerHandler()
{
  DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
  DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
  DefaultHeaders::Instance().addHeader("Referrer-Policy", "no-referrer");
  DefaultHeaders::Instance().addHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
  registerPageRoutes();
  registerStaticAssetRoutes();
  registerApiRoutes();
  registerMaintenanceRoutes();
  registerFallbackRoutes();
}
