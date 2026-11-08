#include "WinSystemPS4.h"
#include "platform/ps4/input/PS4PadInput.h"
#include "platform/ps4/PS4BootLog.h"

#include "cores/VideoPlayer/VideoRenderers/RenderFactory.h"
#include "cores/VideoPlayer/VideoRenderers/LinuxRendererGLES.h"
#include "settings/DisplaySettings.h"
#include "utils/EGLUtils.h"
#include "utils/log.h"
#include "windowing/WindowSystemFactory.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <orbis/libkernel.h>

#include <cstdint>
#include <string>

using namespace KODI::WINDOWING::PS4;

CWinSystemPS4::CWinSystemPS4()
  : KODI::WINDOWING::LINUX::CWinSystemEGL(EGL_NONE, "")
{
}

CWinSystemPS4::~CWinSystemPS4() = default;

void CWinSystemPS4::Register()
{
  KODI::WINDOWING::CWindowSystemFactory::RegisterWindowSystem(CreateWinSystem, "ps4");
}

std::unique_ptr<CWinSystemBase> CWinSystemPS4::CreateWinSystem()
{
  return std::make_unique<CWinSystemPS4>();
}

bool CWinSystemPS4::LoadPigletModules()
{
  PS4BootLog("Loading Piglet modules");
  const char* sandbox = sceKernelGetFsSandboxRandomWord();
  if (!sandbox || !*sandbox)
  {
    PS4BootLog("No sandbox name returned for Piglet module loading");
    CLog::Log(LOGERROR, "PS4: no sandbox name for Piglet modules");
    return false;
  }

  int moduleResult = 0;
  const auto load = [sandbox, &moduleResult](const char* module) {
    const std::string path = std::string("/") + sandbox + "/common/lib/" + module;
    PS4BootLog("Loading system module: %s", path.c_str());
    const int32_t result = sceKernelLoadStartModule(path.c_str(), 0, nullptr, 0, nullptr,
                                                    &moduleResult);
    PS4BootLog("System module result for %s: 0x%08x", module,
               static_cast<uint32_t>(result));
    if (result < 0 && result != static_cast<int32_t>(0x80020008u))
      CLog::Log(LOGERROR, "PS4: loading {} failed: {:#x}", path, result);
    return result >= 0 || result == static_cast<int32_t>(0x80020008u);
  };

  const bool loaded = load("libScePigletv2VSH.sprx") && load("libScePrecompiledShaders.sprx");
  PS4BootLog("Piglet modules: %s", loaded ? "loaded" : "failed");
  return loaded;
}

bool CWinSystemPS4::CreateContext()
{
  PS4BootLog("Creating Piglet EGL display");
  const bool displayCreated = m_eglContext.CreateDisplay(EGL_DEFAULT_DISPLAY);
  PS4BootLog("EGL display creation: %s", displayCreated ? "ok" : "failed");
  const bool displayInitialized =
      displayCreated && m_eglContext.InitializeDisplay(EGL_OPENGL_ES_API);
  PS4BootLog("EGL display initialization: %s", displayInitialized ? "ok" : "failed");
  const bool configChosen = displayInitialized && m_eglContext.ChooseConfig(EGL_OPENGL_ES2_BIT);
  PS4BootLog("EGL config selection: %s", configChosen ? "ok" : "failed");
  if (!configChosen)
  {
    CEGLUtils::Log(LOGERROR, "PS4: Piglet EGL display/config initialization failed");
    return false;
  }

  CEGLAttributesVec contextAttributes;
  contextAttributes.Add({{EGL_CONTEXT_CLIENT_VERSION, 2}});
  const bool contextCreated = m_eglContext.CreateContext(contextAttributes);
  PS4BootLog("EGL context creation: %s", contextCreated ? "ok" : "failed");
  if (!contextCreated)
  {
    CEGLUtils::Log(LOGERROR, "PS4: Piglet GLES2 context creation failed");
    return false;
  }
  PS4BootLog("Piglet EGL context created");
  return true;
}

bool CWinSystemPS4::InitWindowSystem()
{
  PS4BootLog("Window-system initialization entered");
  if (!CWinSystemBase::InitWindowSystem())
  {
    PS4BootLog("Base window-system initialization failed");
    return false;
  }

  VIDEOPLAYER::CRendererFactory::ClearRenderer();
  CLinuxRendererGLES::Register();

  if (!LoadPigletModules())
    return false;

  OrbisPglConfig config{};
  config.size = sizeof(config);
  config.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT |
                 ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
  config.processOrder = 1;
  config.systemSharedMemorySize = 128ull * 1024 * 1024;
  config.videoSharedMemorySize = 256ull * 1024 * 1024;
  config.maxMappedFlexibleMemory = 64ull * 1024 * 1024;
  config.drawCommandBufferSize = 1ull * 1024 * 1024;
  config.lcueResourceBufferSize = 1ull * 1024 * 1024;
  config.dbgPosCmd_0x40 = 1920;
  config.dbgPosCmd_0x44 = 1080;
  config.unk_0x5C = 2;
  // The standalone PS4 probe treats negative SCE results as errors. A zero
  // return is success, so a boolean check would reject successful setup.
  PS4BootLog("Configuring Piglet VideoOut");
  const int32_t pigletResult = scePigletSetConfigurationVSH(&config);
  PS4BootLog("scePigletSetConfigurationVSH: 0x%08x", static_cast<uint32_t>(pigletResult));
  if (pigletResult < 0)
  {
    CLog::Log(LOGERROR, "PS4: scePigletSetConfigurationVSH failed: {:#x}",
              static_cast<uint32_t>(pigletResult));
    return false;
  }

  if (!CreateContext())
  {
    m_eglContext.Destroy();
    return false;
  }
  CLog::Log(LOGINFO, "PS4: Piglet EGL/GLES2 initialized");

  m_padInput = std::make_unique<CPS4PadInput>();
  PS4BootLog("Initializing controller input");
  if (!m_padInput->Initialize())
  {
    CLog::Log(LOGERROR, "PS4: controller input initialization failed");
    m_padInput.reset();
    return false;
  }
  PS4BootLog("Window-system initialization complete");
  return true;
}

bool CWinSystemPS4::DestroyWindowSystem()
{
  if (m_padInput)
  {
    m_padInput->Deinitialize();
    m_padInput.reset();
  }
  DestroyWindow();
  m_eglContext.Destroy();
  VIDEOPLAYER::CRendererFactory::ClearRenderer();
  return CWinSystemBase::DestroyWindowSystem();
}

bool CWinSystemPS4::CreateNewWindow(const std::string& name,
                                   bool fullScreen,
                                   RESOLUTION_INFO& res)
{
  PS4BootLog("Creating Kodi VideoOut surface");
  DestroyWindow();
  m_nWidth = res.iWidth > 0 ? res.iWidth : 1920;
  m_nHeight = res.iHeight > 0 ? res.iHeight : 1080;
  m_nativeWindow = {0, static_cast<uint32_t>(m_nWidth), static_cast<uint32_t>(m_nHeight), 0};

  const bool surfaceCreated = m_eglContext.CreateSurface(&m_nativeWindow);
  PS4BootLog("EGL surface creation: %s", surfaceCreated ? "ok" : "failed");
  const bool contextBound = surfaceCreated && m_eglContext.BindContext();
  PS4BootLog("EGL context bind: %s", contextBound ? "ok" : "failed");
  if (!contextBound)
  {
    CEGLUtils::Log(LOGERROR, "PS4: Piglet EGL surface creation/bind failed");
    m_eglContext.DestroySurface();
    return false;
  }

  m_bWindowCreated = true;
  m_bFullScreen = true;
  UpdateDesktopResolution(res, "PS4", m_nWidth, m_nHeight, 60.0f, 0);
  return true;
}

bool CWinSystemPS4::DestroyWindow()
{
  m_eglContext.DestroySurface();
  m_bWindowCreated = false;
  return true;
}

bool CWinSystemPS4::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  if (newWidth <= 0 || newHeight <= 0)
    return false;
  m_nWidth = newWidth;
  m_nHeight = newHeight;
  m_nativeWindow.uWidth = static_cast<uint32_t>(newWidth);
  m_nativeWindow.uHeight = static_cast<uint32_t>(newHeight);
  return CRenderSystemGLES::ResetRenderSystem(newWidth, newHeight);
}

bool CWinSystemPS4::SetFullScreen(bool fullScreen,
                                  RESOLUTION_INFO& res,
                                  bool blankOtherDisplays)
{
  if (!m_bWindowCreated && !CreateNewWindow("Kodi", true, res))
    return false;
  res.iWidth = m_nWidth;
  res.iHeight = m_nHeight;
  res.fRefreshRate = 60.0f;
  m_bFullScreen = true;
  return CRenderSystemGLES::ResetRenderSystem(m_nWidth, m_nHeight);
}

void CWinSystemPS4::UpdateResolutions()
{
  CWinSystemBase::UpdateResolutions();
  RESOLUTION_INFO& desktop = CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP);
  UpdateDesktopResolution(desktop, "PS4", 1920, 1080, 60.0f, 0);
  CDisplaySettings::GetInstance().ClearCustomResolutions();
  CDisplaySettings::GetInstance().ApplyCalibrations();
}

void CWinSystemPS4::SetVSyncImpl(bool enable)
{
  m_eglContext.SetVSync(enable);
}

void CWinSystemPS4::PresentRenderImpl(bool rendered)
{
  if (rendered && !m_eglContext.TrySwapBuffers())
    CEGLUtils::Log(LOGERROR, "PS4: eglSwapBuffers failed");
}

void CWinSystemPS4::PresentRender(bool rendered, bool videoLayer)
{
  if (!m_bRenderCreated)
    return;
  CRenderSystemGLES::PresentRender(rendered, videoLayer);
}
