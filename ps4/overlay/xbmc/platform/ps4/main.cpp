// xbmc/platform/ps4/main.cpp
// Entry point for PS4MC (Kodi on PS4)
// Based on working VideoOut path from probe v12.

#include "platform/xbmc.h"


#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace
{
void PS4Log(const char* fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  // Write to stdout so the message is available before Kodi logging starts.
  write(1, buf, strlen(buf));
  write(1, "\n", 1);
}
} // namespace

extern "C" int main(int /*argc*/, char* /*argv*/[])
{
  setenv("PYTHONHOME", "/app0", 0);
  setenv("PYTHONNOUSERSITE", "1", 1);
  setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
  setenv("SSL_CERT_FILE", "/app0/share/kodi/system/certs/cacert.pem", 0);
  PS4Log("PS4MC main() starting");

  int status = XBMC_Run(true);

  PS4Log("XBMC_Run returned %d", status);

  return status;
}
