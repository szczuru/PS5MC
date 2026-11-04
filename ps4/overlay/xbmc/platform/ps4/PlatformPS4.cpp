// xbmc/platform/ps4/PlatformPS4.cpp
#include "PlatformPS4.h"
#include "PS4Privileges.h"
#include "windowing/ps4/WinSystemPS4.h"
#include "utils/log.h"

#include <orbis/UserService.h>
#include <orbis/SystemService.h>

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

  sceUserServiceInitialize(nullptr);

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
