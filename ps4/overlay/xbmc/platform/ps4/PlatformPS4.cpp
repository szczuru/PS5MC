// xbmc/platform/ps4/PlatformPS4.cpp
#include "PlatformPS4.h"
#include "PS4BootLog.h"
#include "PS4Privileges.h"
#include "platform/ps4/video/VideoCodecRegistration.h"
#include "platform/ps4/video/VideoDec2PS4.h"
#include "windowing/ps4/WinSystemPS4.h"
#include "utils/log.h"

#include <orbis/UserService.h>
#include <orbis/SystemService.h>

#include <cerrno>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

bool CPlatformPS4::InitStageOne()
{
  PS4BootLog("Platform InitStageOne entered");
  CLog::Log(LOGINFO, "PS4: InitStageOne");

  // Escalate privileges (libXcape / jbc) – needed for disc, full FS, etc.
  auto r = PS4Privileges::Escalate();
  PS4BootLogPromoteToData();
  PS4BootLog("Privilege escalation: %s", PS4Privileges::ResultStr(r));
  CLog::Log(LOGINFO, "PS4: Escalate privileges -> {}", PS4Privileges::ResultStr(r));
  if (r == PS4Privileges::Result::OK || r == PS4Privileges::Result::AlreadyDone)
  {
    PS4BootLog("Mounting optical drive");
    const auto mountResult = PS4Privileges::MountDisc();
    PS4BootLog("Optical mount: %s", PS4Privileges::ResultStr(mountResult));
    CLog::Log(LOGINFO, "PS4: Mount optical drive -> {}",
              PS4Privileges::ResultStr(mountResult));
  }

  // Kodi's POSIX startup requires HOME. PS4 titles do not get one from the
  // launcher; use a writable per-title directory for settings, caches and
  // Python add-on state. /data is also used by the PS4 probe for persistent
  // diagnostics, while /tmp is a fallback for restricted setups.
  constexpr const char* homeCandidates[] = {"/data/kodi", "/tmp/kodi"};
  bool homeSet = false;
  for (const char* home : homeCandidates)
  {
    if (mkdir(home, 0777) != 0 && errno != EEXIST)
      continue;
    struct stat st{};
    if (stat(home, &st) != 0 || !S_ISDIR(st.st_mode) || access(home, W_OK) != 0)
      continue;
    if (setenv("HOME", home, 1) == 0)
    {
      setenv("TMPDIR", home, 1);
      CLog::Log(LOGINFO, "PS4: HOME set to {}", home);
      PS4BootLog("Writable HOME: %s", home);
      homeSet = true;
      break;
    }
  }
  if (!homeSet)
  {
    PS4BootLog("Failed to create writable HOME, errno=%d", errno);
    CLog::Log(LOGERROR, "PS4: could not create a writable HOME (errno {})", errno);
    return false;
  }

  PS4BootLog("Initializing user service");
  const int32_t userServiceResult = sceUserServiceInitialize(nullptr);
  PS4BootLog("User service result: 0x%08x", static_cast<uint32_t>(userServiceResult));

  KODI::PLATFORM::PS4::RegisterVideoCodecs();
  PS4BootLog("PS4 video codecs registered");
  PS4BootLog("Loading Videodec2 module");
  const int32_t videodec = KODI::PLATFORM::PS4::CVideoDec2::LoadModule();
  PS4BootLog("Videodec2 load result: 0x%08x", static_cast<uint32_t>(videodec));
  if (videodec < 0)
    CLog::Log(LOGWARNING, "PS4: Videodec2 module not loaded: {:#x}",
              static_cast<uint32_t>(videodec));

  KODI::WINDOWING::PS4::CWinSystemPS4::Register();
  PS4BootLog("PS4 window system registered");

  const bool result = CPlatformPosix::InitStageOne();
  PS4BootLog("POSIX platform initialization: %s", result ? "ok" : "failed");
  return result;
}

bool CPlatformPS4::InitStageTwo()
{
  CLog::Log(LOGINFO, "PS4: InitStageTwo");
  return CPlatform::InitStageTwo();
}

void CPlatformPS4::DeinitStageOne()
{
  CLog::Log(LOGINFO, "PS4: DeinitStageOne");
  PS4Privileges::UnmountDisc();
  PS4Privileges::Drop();
  CPlatformPosix::DeinitStageOne();
}

// Factory used by Kodi
CPlatform* CPlatform::CreateInstance()
{
  return new CPlatformPS4();
}
