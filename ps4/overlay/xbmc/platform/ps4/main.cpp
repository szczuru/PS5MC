// xbmc/platform/ps4/main.cpp
// Entry point for PS4MC (Kodi on PS4)
// Based on working VideoOut path from probe v12.

#include "platform/xbmc.h"
#include "platform/ps4/PS4BootLog.h"

#include <cstdlib>

extern "C" int main(int /*argc*/, char* /*argv*/[])
{
  setenv("PYTHONHOME", "/app0", 0);
  setenv("PYTHONNOUSERSITE", "1", 1);
  setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
  setenv("SSL_CERT_FILE", "/app0/share/kodi/system/certs/cacert.pem", 0);
  PS4BootLog("PS4 main() starting");

  int status = XBMC_Run(true);

  PS4BootLog("XBMC_Run returned %d", status);

  return status;
}
