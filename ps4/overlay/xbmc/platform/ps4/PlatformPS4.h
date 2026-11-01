// xbmc/platform/ps4/PlatformPS4.h
#pragma once

#include "platform/posix/PlatformPosix.h"

class CPlatformPS4 : public CPlatformPosix
{
public:
  CPlatformPS4() = default;
  ~CPlatformPS4() override = default;

  bool InitStageOne() override;
  bool InitStageTwo() override;
  void DeinitStageOne() override;

  bool IsConfigureAddonsAtStartupEnabled() override { return false; }
};
