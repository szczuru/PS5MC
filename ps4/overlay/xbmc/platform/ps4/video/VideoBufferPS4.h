/*
 *  Copyright (C) 2026 Team Kodi
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "VideoDec2PS4.h"
#include "cores/VideoPlayer/Buffers/VideoBuffer.h"

#include <memory>
#include <mutex>
#include <vector>

namespace KODI::PLATFORM::PS4
{
// The PS4 Kodi GLES renderer currently copies decoded frames. Keep the same
// buffer contract as the PS5 decoder, but do not advertise its GL zero-copy path.
bool IsZeroCopyAvailable();

class CVideoBufferPS4 : public CVideoBuffer
{
public:
  explicit CVideoBufferPS4(int id) : CVideoBuffer(id) {}

  void Set(const std::shared_ptr<CVideoDec2>& decoder, const VideoDec2Picture& picture,
           AVPixelFormat format);
  void ReturnFrame();
  void GetPlanes(uint8_t* (&planes)[YuvImage::MAX_PLANES]) override;
  void GetStrides(int (&strides)[YuvImage::MAX_PLANES]) override;

private:
  std::shared_ptr<CVideoDec2> m_decoder;
  int m_frameIndex{-1};
  const uint8_t* m_data{};
  unsigned m_pitch{};
  unsigned m_height{};
};

class CVideoBufferPoolPS4 : public IVideoBufferPool
{
public:
  ~CVideoBufferPoolPS4() override;
  CVideoBuffer* Get() override;
  void Return(int id) override;

private:
  std::mutex m_mutex;
  std::vector<CVideoBufferPS4*> m_all;
  std::vector<int> m_free;
};
} // namespace KODI::PLATFORM::PS4
