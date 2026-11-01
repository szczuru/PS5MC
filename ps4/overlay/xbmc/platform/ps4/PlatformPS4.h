// xbmc/platform/ps4/PlatformPS4.h
#pragma once

#include "platform/Platform.h"

class CPlatformPS4 : public CPlatform
{
public:
  CPlatformPS4() = default;
  ~CPlatformPS4() override = default;

  bool InitStageOne() override;
  bool InitStageTwo() override;
  void Deinit() override;

  bool IsConfigureAddonsAtStartupEnabled() override { return false; }
};
