#include "func_webServerHandler.h"
#include "webServerRoutes.h"

void WebServerHandler()
{
  registerPageRoutes();
  registerStaticAssetRoutes();
  registerApiRoutes();
  registerMaintenanceRoutes();
  registerFallbackRoutes();
}
