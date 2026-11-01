#include "CPUInfoPS4.h"

CCPUInfoPS4::CCPUInfoPS4()
{
  m_cpuCount = 8; // PS4 Jaguar / PS4 Pro
  m_cpuModel = "AMD Jaguar (PS4)";
}

std::shared_ptr<CCPUInfo> CCPUInfo::GetCPUInfo()
{
  return std::make_shared<CCPUInfoPS4>();
}
