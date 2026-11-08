// xbmc/platform/ps4/main.cpp
// Entry point for PS4MC (Kodi on PS4)
// Based on working VideoOut path from probe v12.

#include "platform/xbmc.h"
#include "platform/ps4/PS4BootLog.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
int g_logfd = -2;
const char* g_logpath = nullptr;

void OpenBootLog()
{
  if (g_logfd != -2)
    return;

  mkdir("/data/kodi", 0777);
  g_logfd = open("/data/kodi/startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (g_logfd >= 0)
  {
    g_logpath = "/data/kodi/startup.log";
    return;
  }
  g_logfd = open("/mnt/usb0/kodi-startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (g_logfd >= 0)
  {
    g_logpath = "/mnt/usb0/kodi-startup.log";
    return;
  }
  if (g_logfd < 0)
  {
    g_logfd = open("/tmp/kodi-startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_logfd >= 0)
      g_logpath = "/tmp/kodi-startup.log";
  }
}
} // namespace

void PS4BootLogPromoteToData()
{
  if (g_logpath && strcmp(g_logpath, "/data/kodi/startup.log") == 0)
    return;

  mkdir("/data/kodi", 0777);
  const int dataLogfd = open("/data/kodi/startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (dataLogfd < 0)
  {
    PS4BootLog("Could not open /data/kodi/startup.log after privilege escalation (errno=%d)",
               errno);
    return;
  }

  if (g_logpath)
  {
    const int previousLogfd = open(g_logpath, O_RDONLY);
    if (previousLogfd >= 0)
    {
      char previousLog[512];
      ssize_t bytesRead;
      while ((bytesRead = read(previousLogfd, previousLog, sizeof(previousLog))) > 0)
        write(dataLogfd, previousLog, static_cast<size_t>(bytesRead));
      close(previousLogfd);
    }
  }

  if (g_logfd >= 0)
    close(g_logfd);
  g_logfd = dataLogfd;
  g_logpath = "/data/kodi/startup.log";
  fsync(g_logfd);
  PS4BootLog("Startup log moved to /data/kodi/startup.log");
}

void PS4BootLog(const char* fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  OpenBootLog();
  // Keep console output for debug sessions and persist every startup stage so
  // a fatal error before Kodi's file logger starts still leaves evidence.
  write(1, buf, strlen(buf));
  write(1, "\n", 1);
  if (g_logfd >= 0)
  {
    write(g_logfd, buf, strlen(buf));
    write(g_logfd, "\n", 1);
    fsync(g_logfd);
  }
}

// Run before ordinary C++ global constructors and main(). If no log appears,
// the failure is in ELF loading/startup or the app cannot write any fallback.
__attribute__((constructor(101))) static void PS4EarlyBootLog()
{
  OpenBootLog();
  PS4BootLog("PS4 early constructor reached; log: %s",
             g_logpath ? g_logpath : "unavailable");
}

extern "C" int main(int /*argc*/, char* /*argv*/[])
{
  setenv("PYTHONHOME", "/app0", 0);
  setenv("PYTHONNOUSERSITE", "1", 1);
  setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
  setenv("SSL_CERT_FILE", "/app0/share/kodi/system/certs/cacert.pem", 0);
  OpenBootLog();
  PS4BootLog("PS4 main() starting; log: %s", g_logpath ? g_logpath : "unavailable");

  int status = XBMC_Run(true);

  PS4BootLog("XBMC_Run returned %d", status);

  return status;
}
