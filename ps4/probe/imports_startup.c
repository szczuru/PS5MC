/* Runtime-loader smoke test generated from Kodi's undefined dynamic symbols. */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

extern void* const g_kodi_imports[];
extern const unsigned long g_kodi_import_count;

static void Log(const char* format, ...)
{
  (void)mkdir("/data/kodi", 0777);
  const int fd = open("/data/kodi/import-probe.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0)
    return;

  char message[256];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  if (length > 0)
  {
    const size_t size = (size_t)length < sizeof(message) ? (size_t)length : sizeof(message) - 1;
    (void)write(fd, message, size);
    (void)write(fd, "\n", 1);
    (void)fsync(fd);
  }
  (void)close(fd);
}

__attribute__((constructor(1))) static void EarlyMarker(void)
{
  Log("IMPORT_PROBE_EARLY_CONSTRUCTOR_REACHED");
}

int main(void)
{
  /* Touch every pointer so the linker retains all Kodi imports in this ELF. */
  volatile void* first_import = g_kodi_import_count ? g_kodi_imports[0] : 0;
  (void)first_import;
  Log("IMPORT_PROBE_MAIN_REACHED count=%lu", g_kodi_import_count);
  for (;;)
    (void)sleep(60);
}
