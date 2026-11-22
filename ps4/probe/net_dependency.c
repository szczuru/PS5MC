/* Minimal loader probe for Kodi's additional libSceNet.so dependency. */
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void log_line(const char* message)
{
  const int fd = open("/data/kodi/net-dependency-probe.log",
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0)
  {
    (void)write(fd, message, strlen(message));
    (void)write(fd, "\n", 1);
    (void)fsync(fd);
    (void)close(fd);
  }
  (void)write(STDOUT_FILENO, message, strlen(message));
  (void)write(STDOUT_FILENO, "\n", 1);
}

__attribute__((constructor(1))) static void early_marker(void)
{
  (void)mkdir("/data/kodi", 0777);
  log_line("NET_PROBE_CONSTRUCTOR_REACHED");
}

int main(void)
{
  log_line("NET_PROBE_MAIN_REACHED");
  log_line("NET_PROBE_IDLE");
  for (;;)
    ;
}
