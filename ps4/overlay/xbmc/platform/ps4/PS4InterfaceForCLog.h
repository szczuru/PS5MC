#pragma once
#include "utils/IPlatformLog.h"

class CPS4InterfaceForCLog : public IPlatformLog
{
public:
  void AddLog(int loglevel, const char* format, va_list args) override;
};
