// xbmc/platform/ps4/PlatformPS4.cpp
#include "PlatformPS4.h"
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
  CLog::Log(LOGINFO, "PS4: InitStageOne");

  // Escalate privileges (libXcape / jbc) – needed for disc, full FS, etc.
  auto r = PS4Privileges::Escalate();
  CLog::Log(LOGINFO, "PS4: Escalate privileges -> {}", PS4Privileges::ResultStr(r));
  if (r == PS4Privileges::Result::OK || r == PS4Privileges::Result::AlreadyDone)
  {
    const auto mountResult = PS4Privileges::MountDisc();
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
      homeSet = true;
      break;
    }
  }
  if (!homeSet)
  {
    CLog::Log(LOGERROR, "PS4: could not create a writable HOME (errno {})", errno);
    return false;
  }

  sceUserServiceInitialize(nullptr);

  KODI::PLATFORM::PS4::RegisterVideoCodecs();
  const int32_t videodec = KODI::PLATFORM::PS4::CVideoDec2::LoadModule();
  if (videodec < 0)
    CLog::Log(LOGWARNING, "PS4: Videodec2 module not loaded: {:#x}",
              static_cast<uint32_t>(videodec));

  KODI::WINDOWING::PS4::CWinSystemPS4::Register();

  return CPlatformPosix::InitStageOne();
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
