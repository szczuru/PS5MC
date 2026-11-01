// xbmc/platform/ps4/main.cpp
// Entry point for PS4MC (Kodi on PS4)
// Based on working VideoOut path from probe v12.

#include "AppParams.h"
#include "Application.h"
#include "ServiceBroker.h"
#include "platform/xbmc.h"
#include "utils/log.h"
#include "windowing/WinEvents.h"

#include "PS4Privileges.h"
#include "VideoOutInfo.h"

#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>

#include <cstdio>
#include <memory>

namespace
{
void PS4Log(const char* fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  // Also write to stdout (captured by some HENs) and CLog when ready
  write(1, buf, strlen(buf));
  write(1, "\n", 1);
}
} // namespace

extern "C" int main(int argc, char* argv[])
{
  PS4Log("PS4MC main() starting");

  // 1. Privileges
  auto priv = PS4Privileges::Escalate();
  PS4Log("Escalate: %s", PS4Privileges::ResultStr(priv));

  sceUserServiceInitialize(nullptr);

  // 2. Create AppParams
  auto params = std::make_shared<CAppParams>();
  // AdvancedSettings / args can be filled later

  // 3. Run Kodi
  // XBMC_Run will eventually call CPlatform::CreateInstance() -> CPlatformPS4
  int status = XBMC_Run(false, params);

  PS4Log("XBMC_Run returned %d", status);

  PS4Privileges::Drop();
  sceSystemServiceLoadExec("exit", nullptr);

  return status;
}
