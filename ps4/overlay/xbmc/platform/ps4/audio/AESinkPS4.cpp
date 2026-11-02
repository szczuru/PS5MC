// xbmc/platform/ps4/audio/AESinkPS4.cpp
// Minimal sink using the working PERSONAL port from probe v12

#include "AESinkPS4.h"
#include "cores/AudioEngine/AESinkFactory.h"
#include "utils/log.h"

#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

#include <algorithm>
#include <cstring>

void CAESinkPS4::Register()
{
  AE::AESinkRegEntry entry;
  entry.sinkName = "PS4";
  entry.createFunc = Create;
  entry.enumerateFunc = EnumerateDevicesEx;
  AE::CAESinkFactory::RegisterSink(entry);
}

std::unique_ptr<IAESink> CAESinkPS4::Create(std::string& device, AEAudioFormat& format)
{
  auto sink = std::make_unique<CAESinkPS4>();
  if (sink->Initialize(format, device))
    return sink;
  return nullptr;
}

void CAESinkPS4::EnumerateDevicesEx(AEDeviceInfoList& list, bool /*force*/)
{
  CAEDeviceInfo info;
  info.m_deviceName = "PERSONAL";
  info.m_displayName = "PS4 Audio";
  info.m_displayNameExtra = "sceAudioOut";
  info.m_deviceType = AE_DEVTYPE_PCM;
  info.m_channels += AE_CH_FL;
  info.m_channels += AE_CH_FR;
  info.m_sampleRates.push_back(48000);
  info.m_dataFormats.push_back(AE_FMT_S16NE);
  info.m_wantsIECPassthrough = false;
  list.push_back(info);
}

bool CAESinkPS4::Initialize(AEAudioFormat& format, std::string& /*device*/)
{
  sceAudioOutInit();

  int32_t user = 0;
  sceUserServiceGetInitialUser(&user);

  // PERSONAL port worked in probe
  m_port = sceAudioOutOpen(user, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                           512, 48000,
                           ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
  if (m_port < 0)
  {
    CLog::Log(LOGERROR, "AESinkPS4: sceAudioOutOpen failed: 0x{:x}", m_port);
    return false;
  }

  format.m_sampleRate = 48000;
  format.m_channelLayout = AE_CH_LAYOUT_2_0;
  format.m_dataFormat = AE_FMT_S16NE;
  format.m_frames = 512;
  format.m_frameSize = 4; // S16 stereo
  m_format = format;

  CLog::Log(LOGINFO, "AESinkPS4: opened PERSONAL @ 48 kHz");
  return true;
}

void CAESinkPS4::Deinitialize()
{
  if (m_port >= 0)
  {
    sceAudioOutClose(m_port);
    m_port = -1;
  }
}

void CAESinkPS4::GetDelay(AEDelayStatus& status)
{
  // Rough estimate – can be improved with sceAudioOutGetStatus later
  status.SetDelay(static_cast<double>(m_format.m_frames) / m_format.m_sampleRate);
}

double CAESinkPS4::GetCacheTotal()
{
  return static_cast<double>(m_format.m_frames) / m_format.m_sampleRate;
}

unsigned int CAESinkPS4::AddPackets(uint8_t** data, unsigned int frames, unsigned int offset)
{
  if (m_port < 0 || !data || !data[0])
    return 0;

  // Interleaved S16 stereo expected
  const short* src = reinterpret_cast<const short*>(data[0] + offset * m_format.m_frameSize);
  // sceAudioOutOutput blocks until space is available
  int r = sceAudioOutOutput(m_port, const_cast<short*>(src));
  if (r < 0)
    return 0;
  return frames;
}

void CAESinkPS4::Drain()
{
  if (m_port >= 0)
    sceAudioOutOutput(m_port, nullptr);
}
