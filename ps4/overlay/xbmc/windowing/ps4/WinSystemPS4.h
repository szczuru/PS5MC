#pragma once

#include <orbis/Pigletv2VSH.h>

#include "rendering/gles/RenderSystemGLES.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

#include <memory>

namespace KODI::WINDOWING::PS4
{

class CWinSystemPS4 final : public CWinSystemBase,
                            public CRenderSystemGLES,
                            public KODI::WINDOWING::LINUX::CWinSystemEGL
{
public:
  CWinSystemPS4();
  ~CWinSystemPS4() override = default;

  static void Register();
  static std::unique_ptr<CWinSystemBase> CreateWinSystem();

  CRenderSystemBase* GetRenderSystem() override { return this; }
  const std::string GetName() override { return "ps4"; }
  bool InitWindowSystem() override;
  bool DestroyWindowSystem() override;
  bool CreateNewWindow(const std::string& name, bool fullScreen, RESOLUTION_INFO& res) override;
  bool DestroyWindow() override;
  bool ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop) override;
  bool SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays) override;
  void UpdateResolutions() override;
  void PresentRender(bool rendered, bool videoLayer) override;
  bool CanDoWindowed() override { return false; }
  bool SupportsScreenMove() override { return false; }
  bool HasCursor() override { return false; }

protected:
  void SetVSyncImpl(bool enable) override;
  void PresentRenderImpl(bool rendered) override;

private:
  bool LoadPigletModules();
  bool CreateContext();

  OrbisPglWindow m_nativeWindow{};
};

} // namespace KODI::WINDOWING::PS4
