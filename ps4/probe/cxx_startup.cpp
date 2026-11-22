// Minimal C++/TLS startup diagnostic. This intentionally avoids Kodi and
// libc++ so it isolates the OpenOrbis CRT, init_array and ELF TLS handling.
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static thread_local volatile int g_threadValue = 0x4b4f4449;

static void Log(const char* message)
{
  (void)mkdir("/data/kodi", 0777);
  const int fd = open("/data/kodi/cxx-startup-probe.log",
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0)
  {
    (void)write(fd, message, __builtin_strlen(message));
    (void)write(fd, "\n", 1);
    (void)fsync(fd);
    (void)close(fd);
  }
}

__attribute__((constructor(101))) static void BeforeCxxInitializers()
{
  Log("CXX_PROBE_PRIORITY_101_REACHED");
}

__attribute__((constructor(200))) static void CxxInitializer()
{
  Log("CXX_PROBE_NORMAL_CONSTRUCTOR_REACHED");
}

extern "C" int main(int, char**)
{
  Log("CXX_PROBE_MAIN_REACHED");
  if (g_threadValue == 0x4b4f4449)
    Log("CXX_PROBE_TLS_READ_OK");
  else
    Log("CXX_PROBE_TLS_READ_BAD");
  return 0;
}
