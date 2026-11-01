// xbmc/platform/ps4/VideoOutInfo.h
#pragma once

#include <cstdint>
#include <vector>

// Thin wrapper around the working VideoOut path discovered by probe v12.
// Primary path: 720p / 1080p linear A8B8G8R8_SRGB + DirectMemory.

class CVideoOutInfo
{
public:
  static CVideoOutInfo& GetInstance();

  bool Open(int width = 1280, int height = 720);
  void Close();

  bool IsOpen() const { return m_handle >= 0; }
  int  GetHandle() const { return m_handle; }
  int  Width() const { return m_width; }
  int  Height() const { return m_height; }

  // Returns a CPU-writable pointer to the current back buffer
  uint32_t* GetBackBuffer();
  // Submit the current back buffer and advance
  bool Flip();

private:
  CVideoOutInfo() = default;
  ~CVideoOutInfo() { Close(); }

  int      m_handle = -1;
  int      m_width = 0;
  int      m_height = 0;
  int      m_current = 0;
  static constexpr int NUM_BUFFERS = 3;

  void*    m_base = nullptr;
  size_t   m_fbSize = 0;
  uint32_t* m_buffers[NUM_BUFFERS] = {};
};
