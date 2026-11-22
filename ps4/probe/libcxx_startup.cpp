/* Isolate Kodi's first libc++ iostream initializer on PS4. */
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

static void Log(const char* message)
{
  (void)mkdir("/data/kodi", 0777);
  const int fd = open("/data/kodi/libcxx-startup-probe.log",
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0)
  {
    const size_t size = __builtin_strlen(message);
    (void)write(fd, message, size);
    (void)write(fd, "\n", 1);
    (void)fsync(fd);
    (void)close(fd);
  }
}

__attribute__((constructor(1))) static void EarlyMarker()
{
  Log("LIBCXX_PROBE_EARLY_CONSTRUCTOR_REACHED");
}

int main()
{
  Log("LIBCXX_PROBE_MAIN_REACHED");
  std::cout << "libc++ iostream initialization succeeded" << std::endl;
  Log("LIBCXX_PROBE_IDLE");
  for (;;)
    (void)sleep(60);
}
