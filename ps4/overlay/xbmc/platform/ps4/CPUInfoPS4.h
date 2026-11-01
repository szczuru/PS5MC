#pragma once
#include "utils/CPUInfo.h"

class CCPUInfoPS4 : public CCPUInfo
{
public:
  CCPUInfoPS4();
  bool SupportsCPUUsage() const override { return false; }
  float GetCPUUsage() override { return 0.f; }
};
