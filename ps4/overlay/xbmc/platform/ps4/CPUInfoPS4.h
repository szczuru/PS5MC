#pragma once
#include "utils/CPUInfo.h"

class CCPUInfoPS4 : public CCPUInfo
{
public:
  CCPUInfoPS4();
  bool SupportsCPUUsage() const override { return false; }
  int GetUsedPercentage() override { return 0; }
  float GetCPUFrequency() override { return 0.f; }
  bool GetTemperature(CTemperature& temperature) override { return false; }
};
