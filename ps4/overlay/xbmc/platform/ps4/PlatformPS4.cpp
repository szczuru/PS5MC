// xbmc/platform/ps4/PlatformPS4.cpp
#include "PlatformPS4.h"
#include "PS4Privileges.h"
#include "utils/log.h"

#include <orbis/UserService.h>
#include <orbis/SystemService.h>

bool CPlatformPS4::InitStageOne()
{
  CLog::Log(LOGINFO, "PS4: InitStageOne");

  // Escalate privileges (libXcape / jbc) – needed for disc, full FS, etc.
  auto r = PS4Privileges::Escalate();
  CLog::Log(LOGINFO, "PS4: Escalate privileges -> {}", PS4Privileges::ResultStr(r));

  sceUserServiceInitialize(nullptr);

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
  PS4Privileges::Drop();
  CPlatformPosix::DeinitStageOne();
}

// Factory used by Kodi
CPlatform* CPlatform::CreateInstance()
{
  return new CPlatformPS4();
}
