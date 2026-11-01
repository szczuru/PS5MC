// xbmc/platform/ps4/VideoOutInfo.cpp
// Working path from probe v12: 720p/1080p linear A8B8G8R8_SRGB + DirectMemory

#include "VideoOutInfo.h"
#include "utils/log.h"

#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

#include <cstring>

CVideoOutInfo& CVideoOutInfo::GetInstance()
{
  static CVideoOutInfo inst;
  return inst;
}

bool CVideoOutInfo::Open(int width, int height)
{
  if (m_handle >= 0)
    return true;

  int32_t user = 0;
  sceUserServiceGetInitialUser(&user);

  m_handle = sceVideoOutOpen(user, 0 /* MAIN */, 0, nullptr);
  if (m_handle < 0)
  {
    CLog::Log(LOGERROR, "VideoOut: sceVideoOutOpen failed: 0x{:x}", m_handle);
    return false;
  }

  sceVideoOutSetFlipRate(m_handle, 0);

  m_width = width;
  m_height = height;
  m_fbSize = static_cast<size_t>(width) * height * 4;

  const size_t align = 2 * 1024 * 1024;
  size_t total = ((m_fbSize * NUM_BUFFERS) + align - 1) & ~(align - 1);

  off_t phys = 0;
  int r = static_cast<int>(sceKernelAllocateDirectMemory(
      0, static_cast<off_t>(0x200000000LL), total, align, 3, &phys));
  if (r != 0)
  {
    CLog::Log(LOGERROR, "VideoOut: AllocateDirectMemory failed: 0x{:x}", r);
    Close();
    return false;
  }

  r = static_cast<int>(sceKernelMapDirectMemory(
      &m_base, total, 0x33, 0, phys, align));
  if (r != 0 || !m_base)
  {
    CLog::Log(LOGERROR, "VideoOut: MapDirectMemory failed: 0x{:x}", r);
    Close();
    return false;
  }

  for (int i = 0; i < NUM_BUFFERS; i++)
    m_buffers[i] = reinterpret_cast<uint32_t*>(
        static_cast<uint8_t*>(m_base) + static_cast<size_t>(i) * m_fbSize);

  OrbisVideoOutBufferAttribute attr{};
  sceVideoOutSetBufferAttribute(&attr,
      0x80002200 /* A8B8G8R8_SRGB */,
      1 /* LINEAR */,
      0 /* 16:9 */,
      width, height, width);

  r = sceVideoOutRegisterBuffers(m_handle, 0,
                                 reinterpret_cast<void* const*>(m_buffers),
                                 NUM_BUFFERS, &attr);
  if (r < 0)
  {
    CLog::Log(LOGERROR, "VideoOut: RegisterBuffers failed: 0x{:x}", r);
    Close();
    return false;
  }

  m_current = 0;
  CLog::Log(LOGINFO, "VideoOut: opened {}x{} linear A8B8G8R8", width, height);
  return true;
}

void CVideoOutInfo::Close()
{
  if (m_handle >= 0)
  {
    sceVideoOutClose(m_handle);
    m_handle = -1;
  }
  // Note: we intentionally do not free DirectMemory on exit
  // (some firmwares are unhappy about it)
  m_base = nullptr;
  m_width = m_height = 0;
}

uint32_t* CVideoOutInfo::GetBackBuffer()
{
  if (m_handle < 0)
    return nullptr;
  return m_buffers[m_current];
}

bool CVideoOutInfo::Flip()
{
  if (m_handle < 0)
    return false;

  int r = sceVideoOutSubmitFlip(m_handle, m_current, 1 /* VSYNC */, 0);
  if (r < 0)
    return false;

  // simple wait
  for (int i = 0; i < 50; i++)
  {
    if (sceVideoOutIsFlipPending(m_handle) == 0)
      break;
    sceKernelUsleep(1000);
  }

  m_current = (m_current + 1) % NUM_BUFFERS;
  return true;
}
