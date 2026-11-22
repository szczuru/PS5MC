/*
 * Establish whether the PS4 loader invokes DT_PREINIT_ARRAY before the CRT's
 * DT_INIT and the executable's ordinary constructor array.
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void Log(const char* message)
{
  (void)mkdir("/data/kodi", 0777);
  const int fd = open("/data/kodi/preinit-startup-probe.log",
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0)
  {
    (void)write(fd, message, __builtin_strlen(message));
    (void)write(fd, "\n", 1);
    (void)fsync(fd);
    (void)close(fd);
  }
}

static void BeforeSharedObjectInitializers(void)
{
  Log("PREINIT_ARRAY_REACHED");
}

__attribute__((section(".preinit_array"), used))
static void (*const g_preinit[])(void) = {BeforeSharedObjectInitializers};

__attribute__((constructor(1))) static void AfterDtInit(void)
{
  Log("INIT_ARRAY_REACHED");
}

int main(void)
{
  Log("MAIN_REACHED");
  for (;;)
    (void)sleep(60);
}
