#include "PS4InterfaceForCLog.h"
#include <cstdio>
#include <unistd.h>

void CPS4InterfaceForCLog::AddLog(int /*loglevel*/, const char* format, va_list args)
{
  char buf[1024];
  vsnprintf(buf, sizeof(buf), format, args);
  size_t n = strlen(buf);
  write(1, buf, n);
  write(1, "\n", 1);
}
