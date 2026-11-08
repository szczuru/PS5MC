#pragma once

// Persistent low-level log for diagnosing crashes before Kodi's own logger is
// initialized. The file is written to /data/kodi/startup.log when possible.
#ifdef __cplusplus
extern "C" {
#endif

void PS4BootLog(const char* format, ...);
void PS4BootLogPromoteToData();

#ifdef __cplusplus
}
#endif
