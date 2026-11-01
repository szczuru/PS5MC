// xbmc/platform/ps4/input/PS4PadInput.cpp
#include "PS4PadInput.h"
#include "input/InputManager.h"
#include "ServiceBroker.h"
#include "utils/log.h"

#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <cstring>

CPS4PadInput::CPS4PadInput() : CThread("PS4Pad") {}

CPS4PadInput::~CPS4PadInput()
{
  Deinitialize();
}

bool CPS4PadInput::Initialize()
{
  scePadInit();
  int32_t user = 0;
  sceUserServiceGetInitialUser(&user);
  m_handle = scePadOpen(user, 0, 0, nullptr);
  if (m_handle < 0)
  {
    CLog::Log(LOGERROR, "PS4Pad: scePadOpen failed: 0x{:x}", m_handle);
    return false;
  }
  m_running = true;
  Create();
  CLog::Log(LOGINFO, "PS4Pad: opened");
  return true;
}

void CPS4PadInput::Deinitialize()
{
  m_running = false;
  StopThread();
  if (m_handle >= 0)
  {
    scePadClose(m_handle);
    m_handle = -1;
  }
}

void CPS4PadInput::Process()
{
  OrbisPadData prev{};
  while (m_running)
  {
    OrbisPadData data{};
    if (scePadReadState(m_handle, &data) == 0)
    {
      // Very basic mapping – expand later
      auto& im = CServiceBroker::GetInputManager();
      // Example: Cross = Select / A
      // (real mapping should go through CJoystick or keymap system)
      (void)im;
      (void)prev;
      prev = data;
    }
    sceKernelUsleep(8000); // ~120 Hz
  }
}
