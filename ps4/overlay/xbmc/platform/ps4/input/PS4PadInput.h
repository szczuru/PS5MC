// xbmc/platform/ps4/input/PS4PadInput.h
#pragma once

#include "threads/Thread.h"

#include <atomic>

class CPS4PadInput : public CThread
{
public:
  CPS4PadInput();
  ~CPS4PadInput() override;

  bool Initialize();
  void Deinitialize();

protected:
  void Process() override;

private:
  int m_handle = -1;
  std::atomic<bool> m_running{false};
};
