#pragma once

// Persistent low-level log for diagnosing crashes before Kodi's own logger is
// initialized. The file is written to /data/kodi/startup.log when possible.
void PS4BootLog(const char* format, ...);
