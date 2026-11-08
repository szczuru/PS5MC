// Early PS4 startup logger kept in C so its constructor does not depend on
// Kodi's C++ runtime initialization.
#include "PS4BootLog.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_logfd = -2;
static const char* g_logpath;

static void OpenBootLog(void)
{
  if (g_logfd != -2)
    return;

  (void)mkdir("/data/kodi", 0777);
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

  g_logfd = open("/mnt/usb1/kodi-startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (g_logfd >= 0)
  {
    g_logpath = "/mnt/usb1/kodi-startup.log";
    return;
  }

  g_logfd = open("/tmp/kodi-startup.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (g_logfd >= 0)
    g_logpath = "/tmp/kodi-startup.log";
}

void PS4BootLog(const char* format, ...)
{
  char buffer[512];
  va_list args;
  va_start(args, format);
  (void)vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  OpenBootLog();
  (void)write(STDOUT_FILENO, buffer, strlen(buffer));
  (void)write(STDOUT_FILENO, "\n", 1);
  if (g_logfd >= 0)
  {
    (void)write(g_logfd, buffer, strlen(buffer));
    (void)write(g_logfd, "\n", 1);
    (void)fsync(g_logfd);
  }
}

void PS4BootLogPromoteToData(void)
{
  if (g_logpath && strcmp(g_logpath, "/data/kodi/startup.log") == 0)
    return;

  (void)mkdir("/data/kodi", 0777);
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
        (void)write(dataLogfd, previousLog, (size_t)bytesRead);
      (void)close(previousLogfd);
    }
  }

  if (g_logfd >= 0)
    (void)close(g_logfd);
  g_logfd = dataLogfd;
  g_logpath = "/data/kodi/startup.log";
  (void)fsync(g_logfd);
  PS4BootLog("Startup log moved to /data/kodi/startup.log");
}

__attribute__((constructor(101))) static void PS4EarlyBootLog(void)
{
  OpenBootLog();
  PS4BootLog("PS4 C early constructor reached; log: %s",
             g_logpath ? g_logpath : "unavailable");
}
