// xbmc/platform/ps4/audio/AESinkPS4.h
#pragma once

#include "cores/AudioEngine/Interfaces/AESink.h"
#include "cores/AudioEngine/Utils/AEDeviceInfo.h"

class CAESinkPS4 : public IAESink
{
public:
  const char* GetName() override { return "PS4"; }

  static void Register();
  static std::unique_ptr<IAESink> Create(std::string& device, AEAudioFormat& format);
  static void EnumerateDevicesEx(AEDeviceInfoList& list, bool force = false);

  bool Initialize(AEAudioFormat& format, std::string& device) override;
  void Deinitialize() override;

  void GetDelay(AEDelayStatus& status) override;
  double GetCacheTotal() override;
  unsigned int AddPackets(uint8_t** data, unsigned int frames, unsigned int offset) override;
  void Drain() override;

private:
  int m_port = -1;
  AEAudioFormat m_format;
};
