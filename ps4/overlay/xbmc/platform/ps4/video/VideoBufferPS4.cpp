/*
 *  Copyright (C) 2026 Team Kodi
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "VideoBufferPS4.h"

namespace KODI::PLATFORM::PS4
{
bool IsZeroCopyAvailable()
{
  return false;
}

void CVideoBufferPS4::Set(const std::shared_ptr<CVideoDec2>& decoder,
                          const VideoDec2Picture& picture, AVPixelFormat format)
{
  m_decoder = decoder;
  m_frameIndex = picture.frameIndex;
  m_data = picture.data;
  m_pitch = picture.pitch;
  m_height = picture.height;
  m_pixFormat = format;
}

void CVideoBufferPS4::ReturnFrame()
{
  if (m_decoder && m_frameIndex >= 0)
    m_decoder->ReleaseFrame(m_frameIndex);
  m_decoder.reset();
  m_frameIndex = -1;
  m_data = nullptr;
}

void CVideoBufferPS4::GetPlanes(uint8_t* (&planes)[YuvImage::MAX_PLANES])
{
  planes[0] = const_cast<uint8_t*>(m_data);
  planes[1] = m_data ? const_cast<uint8_t*>(m_data + static_cast<size_t>(m_pitch) * m_height)
                     : nullptr;
  planes[2] = nullptr;
}

void CVideoBufferPS4::GetStrides(int (&strides)[YuvImage::MAX_PLANES])
{
  strides[0] = static_cast<int>(m_pitch);
  strides[1] = static_cast<int>(m_pitch);
  strides[2] = 0;
}

CVideoBufferPoolPS4::~CVideoBufferPoolPS4()
{
  for (CVideoBufferPS4* buffer : m_all)
    delete buffer;
}

CVideoBuffer* CVideoBufferPoolPS4::Get()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  CVideoBufferPS4* buffer;
  if (!m_free.empty())
  {
    buffer = m_all[m_free.back()];
    m_free.pop_back();
  }
  else
  {
    buffer = new CVideoBufferPS4(static_cast<int>(m_all.size()));
    m_all.push_back(buffer);
  }
  lock.unlock();
  buffer->Acquire(GetPtr());
  return buffer;
}

void CVideoBufferPoolPS4::Return(int id)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (id < 0 || id >= static_cast<int>(m_all.size()))
    return;
  m_all[id]->ReturnFrame();
  m_free.push_back(id);
}
} // namespace KODI::PLATFORM::PS4
