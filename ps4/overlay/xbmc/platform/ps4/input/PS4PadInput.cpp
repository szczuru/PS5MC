// xbmc/platform/ps4/input/PS4PadInput.cpp
#include "PS4PadInput.h"

#include "ServiceBroker.h"
#include "application/AppInboundProtocol.h"
#include "input/keyboard/XBMC_keyboard.h"
#include "input/keyboard/XBMC_keysym.h"
#include "utils/log.h"
#include "windowing/XBMC_events.h"

#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <chrono>

namespace
{
constexpr uint32_t DIRECTION_BITS = ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN |
                                   ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT;
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(8);
constexpr auto REPEAT_DELAY = std::chrono::milliseconds(400);
constexpr auto REPEAT_RATE = std::chrono::milliseconds(80);
constexpr uint8_t STICK_DEADZONE = 64;

uint32_t StickToDpad(uint8_t x, uint8_t y)
{
  uint32_t buttons = 0;
  if (x < 128 - STICK_DEADZONE)
    buttons |= ORBIS_PAD_BUTTON_LEFT;
  else if (x > 128 + STICK_DEADZONE)
    buttons |= ORBIS_PAD_BUTTON_RIGHT;
  if (y < 128 - STICK_DEADZONE)
    buttons |= ORBIS_PAD_BUTTON_UP;
  else if (y > 128 + STICK_DEADZONE)
    buttons |= ORBIS_PAD_BUTTON_DOWN;
  return buttons;
}

uint16_t ButtonToKeysym(uint32_t button)
{
  switch (button)
  {
    case ORBIS_PAD_BUTTON_UP:
      return XBMCK_UP;
    case ORBIS_PAD_BUTTON_DOWN:
      return XBMCK_DOWN;
    case ORBIS_PAD_BUTTON_LEFT:
      return XBMCK_LEFT;
    case ORBIS_PAD_BUTTON_RIGHT:
      return XBMCK_RIGHT;
    case ORBIS_PAD_BUTTON_CROSS:
      return XBMCK_RETURN;
    case ORBIS_PAD_BUTTON_CIRCLE:
      return XBMCK_BACKSPACE;
    case ORBIS_PAD_BUTTON_TRIANGLE:
      return XBMCK_c;
    case ORBIS_PAD_BUTTON_SQUARE:
      return XBMCK_i;
    case ORBIS_PAD_BUTTON_OPTIONS:
      return XBMCK_MEDIA_PLAY_PAUSE;
    case ORBIS_PAD_BUTTON_L1:
      return XBMCK_PAGEUP;
    case ORBIS_PAD_BUTTON_R1:
      return XBMCK_PAGEDOWN;
    case ORBIS_PAD_BUTTON_L2:
      return XBMCK_r;
    case ORBIS_PAD_BUTTON_R2:
      return XBMCK_f;
    case ORBIS_PAD_BUTTON_L3:
      return XBMCK_o;
    case ORBIS_PAD_BUTTON_R3:
      return XBMCK_m;
    case ORBIS_PAD_BUTTON_TOUCH_PAD:
      return XBMCK_ESCAPE;
    default:
      return 0;
  }
}

void EmitKey(uint32_t button, bool down)
{
  const uint16_t sym = ButtonToKeysym(button);
  if (!sym)
    return;

  XBMC_Event event{};
  event.type = down ? XBMC_KEYDOWN : XBMC_KEYUP;
  event.key.keysym.scancode = sym;
  event.key.keysym.sym = static_cast<XBMCKey>(sym);
  event.key.keysym.mod = button == ORBIS_PAD_BUTTON_L3
                             ? static_cast<XBMCMod>(XBMCKMOD_LCTRL | XBMCKMOD_LSHIFT)
                             : static_cast<XBMCMod>(0);
  event.key.keysym.unicode = 0;

  if (const auto appPort = CServiceBroker::GetAppPort())
    appPort->OnEvent(event);
}
} // namespace

CPS4PadInput::CPS4PadInput() : CThread("PS4Pad")
{
}

CPS4PadInput::~CPS4PadInput()
{
  Deinitialize();
}

bool CPS4PadInput::Initialize()
{
  int result = scePadInit();
  if (result != 0)
  {
    CLog::Log(LOGERROR, "PS4Pad: scePadInit failed: {:#x}", static_cast<uint32_t>(result));
    return false;
  }

  int32_t user = -1;
  result = sceUserServiceGetInitialUser(&user);
  if (result != 0)
  {
    CLog::Log(LOGERROR, "PS4Pad: sceUserServiceGetInitialUser failed: {:#x}",
              static_cast<uint32_t>(result));
    return false;
  }

  m_handle = scePadOpen(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
  if (m_handle < 0)
  {
    CLog::Log(LOGERROR, "PS4Pad: scePadOpen failed: {:#x}", static_cast<uint32_t>(m_handle));
    return false;
  }

  m_running = true;
  Create();
  CLog::Log(LOGINFO, "PS4Pad: opened controller for user {}", user);
  return true;
}

void CPS4PadInput::Deinitialize()
{
  m_running = false;
  StopThread(true);
  if (m_handle >= 0)
  {
    scePadClose(m_handle);
    m_handle = -1;
  }
}

void CPS4PadInput::Process()
{
  uint32_t lastButtons = 0;
  uint32_t heldRepeat = 0;
  auto nextRepeat = std::chrono::steady_clock::time_point{};

  while (m_running)
  {
    OrbisPadData data{};
    uint32_t buttons = 0;
    if (scePadReadState(m_handle, &data) == 0 && data.connected)
    {
      buttons = data.buttons;
      if (!(buttons & DIRECTION_BITS))
        buttons |= StickToDpad(data.leftStick.x, data.leftStick.y);
    }

    const auto now = std::chrono::steady_clock::now();
    const uint32_t changed = buttons ^ lastButtons;
    for (uint32_t bit = 1; bit; bit <<= 1)
    {
      if (!(changed & bit))
        continue;
      const bool down = (buttons & bit) != 0;
      EmitKey(bit, down);
      if (bit & DIRECTION_BITS)
      {
        if (down)
        {
          heldRepeat = bit;
          nextRepeat = now + REPEAT_DELAY;
        }
        else if (heldRepeat == bit)
          heldRepeat = 0;
      }
    }

    if (heldRepeat && (buttons & heldRepeat) && now >= nextRepeat)
    {
      EmitKey(heldRepeat, true);
      nextRepeat = now + REPEAT_RATE;
    }

    lastButtons = buttons;
    sceKernelUsleep(static_cast<uint32_t>(POLL_INTERVAL.count() * 1000));
  }

  // Ensure Kodi never keeps a key held if this thread is stopped mid-press.
  for (uint32_t bit = 1; bit; bit <<= 1)
    if (lastButtons & bit)
      EmitKey(bit, false);
}
