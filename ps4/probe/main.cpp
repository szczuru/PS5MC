// PS4MC probe - first on-console test for the Kodi PS4 port.
//
// Does NOT run Kodi. It answers the open questions of the port with data:
//   1. Does Sony's Piglet (GLES 2.0 / EGL 1.4) work from this title? Which
//      extensions does it expose? How fast is a 1080p YUV texture upload
//      (the software video path)?
//   2. Are the hardware video decoder modules present (Videodec, Videodec2,
//      VdecCore, AvPlayer)? Which symbols resolve?
//   3. Which uid / filesystem access do we have (/dev/cd0, /dev/da0, /data,
//      USB)? Can we read the first sectors of a disc?
//   4. Do pad and audio output work?
//
// Log: first of /data/ps4mc/probe.log, /mnt/usb0/ps4mc-probe.log, /tmp/ps4mc-probe.log

#include <stdlib.h> // before <cmath>: libc++ needs ::abs
#include "PS4Privileges.h"
#include <cstddef>
#include <cstdint>

#include <orbis/Pigletv2VSH.h> // must come before any GL/EGL include
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>

#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static FILE* g_log = nullptr;

static void Log(const char* fmt, ...)
{
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  std::fputs(buf, stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
  if (g_log)
  {
    std::fputs(buf, g_log);
    std::fputc('\n', g_log);
    std::fflush(g_log);
  }
}

static void OpenLog()
{
  const char* paths[] = {"/data/ps4mc/probe.log", "/mnt/usb0/ps4mc-probe.log",
                         "/tmp/ps4mc-probe.log"};
  mkdir("/data/ps4mc", 0777);
  for (const char* p : paths)
  {
    g_log = std::fopen(p, "w");
    if (g_log)
    {
      Log("log file: %s", p);
      return;
    }
  }
}

static double NowMs()
{
  return static_cast<double>(sceKernelGetProcessTime()) / 1000.0;
}

// ---------------------------------------------------------------- system
static void ProbeSystem()
{
  Log("== system ==");
  Log("uid=%d euid=%d gid=%d", static_cast<int>(getuid()), static_cast<int>(geteuid()),
      static_cast<int>(getgid()));
  Log("sandbox word: %s", sceKernelGetFsSandboxRandomWord());
  size_t flex = 0;
  // The toolchain header declares this with a by-value size_t; the real
  // signature takes a pointer. Call it through a cast.
  auto availFlex = reinterpret_cast<int32_t (*)(size_t*)>(&sceKernelAvailableFlexibleMemorySize);
  int r = availFlex(&flex);
  Log("flexible memory available: %zu MiB (ret 0x%x)", flex / (1024 * 1024), r);
  Log("direct memory size: %zu MiB",
      static_cast<size_t>(sceKernelGetDirectMemorySize()) / (1024 * 1024));

  const char* dirs[] = {"/app0",
                        "/data",
                        "/mnt/usb0",
                        "/mnt/usb1",
                        "/system/common/lib",
                        "/system_ex/common_ex/lib",
                        "/dev"};
  for (const char* d : dirs)
  {
    struct stat st;
    int e = stat(d, &st) == 0 ? 0 : errno;
    Log("stat %-28s -> %s", d, e == 0 ? "ok" : strerror(e));
  }
}

static void ProbeDisc()
{
  Log("== disc / block devices ==");
  const char* devs[] = {"/dev/cd0", "/dev/da0", "/dev/da1"};
  for (const char* d : devs)
  {
    int fd = open(d, O_RDONLY);
    if (fd < 0)
    {
      Log("open %s: %s", d, strerror(errno));
      continue;
    }
    Log("open %s: ok (fd %d)", d, fd);
    if (std::strcmp(d, "/dev/cd0") == 0)
    {
      // ISO 9660 primary volume descriptor lives at sector 16 (2048 B/sector)
      std::vector<unsigned char> sec(2048);
      off_t o = lseek(fd, static_cast<off_t>(16) * 2048, SEEK_SET);
      ssize_t n = read(fd, sec.data(), sec.size());
      if (o < 0 || n != 2048)
        Log("  read sector 16: %zd (%s)", n, strerror(errno));
      else
        Log("  sector 16: type=%u id='%.5s' (CD001 = ISO9660 ok)", sec[0],
            reinterpret_cast<const char*>(&sec[1]));
    }
    close(fd);
  }
}

// ----------------------------------------------------- decoder modules
static void ProbeDecoders()
{
  Log("== video decoder modules ==");
  struct Mod
  {
    const char* file;
    std::vector<const char*> syms;
  };
  std::vector<Mod> mods = {
      {"libSceVideodec.sprx",
       {"sceVideodecQueryResourceInfo", "sceVideodecCreateDecoder", "sceVideodecDecode",
        "sceVideodecFlush", "sceVideodecReset", "sceVideodecDeleteDecoder",
        "sceVideodecMapMemory"}},
      {"libSceVideodec2.sprx",
       {"sceVideodec2QueryDecoderMemoryInfo", "sceVideodec2CreateDecoder",
        "sceVideodec2CreateHevcDecoder", "sceVideodec2Decode", "sceVideodec2GetPictureInfo",
        "sceVideodec2AllocateComputeQueue", "sceVideodec2Flush", "sceVideodec2Reset",
        "sceVideodec2DeleteDecoder"}},
      {"libSceVdecCore.sprx", {}},
      {"libSceAvPlayer.sprx", {"sceAvPlayerInit", "sceAvPlayerAddSource", "sceAvPlayerGetVideoData"}},
      {"libSceDiscMap.sprx", {}},
  };
  const std::string word = sceKernelGetFsSandboxRandomWord();
  for (const Mod& m : mods)
  {
    const char* dirs[] = {"common/lib/", "common_ex/lib/"};
    uint32_t h = 0xffffffff;
    for (const char* d : dirs)
    {
      std::string path = "/" + word + "/" + d + m.file;
      int ms = 0;
      h = sceKernelLoadStartModule(path.c_str(), 0, nullptr, 0, nullptr, &ms);
      if (static_cast<int32_t>(h) >= 0)
        break;
    }
    if (static_cast<int32_t>(h) < 0)
    {
      Log("%-24s load FAILED 0x%08x", m.file, h);
      continue;
    }
    Log("%-24s loaded, handle %d", m.file, static_cast<int>(h));
    for (const char* s : m.syms)
    {
      void* addr = nullptr;
      int r = sceKernelDlsym(static_cast<int32_t>(h), s, &addr);
      Log("    %-40s %s", s, (r == 0 && addr) ? "resolved" : "NOT FOUND");
    }
  }
}

// ----------------------------------------------------------- Piglet GL
static bool g_gl_ok = false;

static void ProbeGL()
{
  Log("== Piglet (GLES2/EGL) ==");
  const std::string prefix = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/lib/";
  int ms = 0;
  uint32_t piglet = sceKernelLoadStartModule((prefix + "libScePigletv2VSH.sprx").c_str(), 0,
                                             nullptr, 0, nullptr, &ms);
  Log("libScePigletv2VSH: 0x%08x", piglet);
  if (static_cast<int32_t>(piglet) < 0)
    return;
  uint32_t shaders = sceKernelLoadStartModule((prefix + "libScePrecompiledShaders.sprx").c_str(),
                                              0, nullptr, 0, nullptr, &ms);
  Log("libScePrecompiledShaders: 0x%08x", shaders);
  // Sony's runtime shader compiler. Piglet may need it for glCompileShader.
  for (const char* dir : {"common/lib/", "common_ex/lib/"})
  {
    std::string path = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/" + dir + "libSceShaccVSH.sprx";
    uint32_t sh = sceKernelLoadStartModule(path.c_str(), 0, nullptr, 0, nullptr, &ms);
    Log("libSceShaccVSH (%s): 0x%08x", dir, sh);
    if (static_cast<int32_t>(sh) >= 0)
      break;
  }

  const int W = 1920, H = 1080;
  OrbisPglConfig cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.size = sizeof(cfg);
  cfg.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT | ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
  cfg.processOrder = 1;
  cfg.systemSharedMemorySize = 250ull * 1024 * 1024;
  cfg.videoSharedMemorySize = 512ull * 1024 * 1024;
  cfg.maxMappedFlexibleMemory = 170ull * 1024 * 1024;
  cfg.drawCommandBufferSize = 1 * 1024 * 1024;
  cfg.lcueResourceBufferSize = 1 * 1024 * 1024;
  cfg.dbgPosCmd_0x40 = W;
  cfg.dbgPosCmd_0x44 = H;
  cfg.dbgPosCmd_0x48 = 0;
  cfg.dbgPosCmd_0x4C = 0;
  cfg.unk_0x5C = 2;
  if (!scePigletSetConfigurationVSH(&cfg))
  {
    Log("scePigletSetConfigurationVSH failed");
    return;
  }

  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj = 0, min = 0;
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min))
  {
    Log("eglInitialize failed 0x%x", eglGetError());
    return;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  eglSwapInterval(dpy, 1);
  const EGLint attribs[] = {EGL_RED_SIZE,          8,
                            EGL_GREEN_SIZE,        8,
                            EGL_BLUE_SIZE,         8,
                            EGL_ALPHA_SIZE,        8,
                            EGL_DEPTH_SIZE,        0,
                            EGL_STENCIL_SIZE,      0,
                            EGL_SAMPLE_BUFFERS,    0,
                            EGL_SAMPLES,           0,
                            EGL_RENDERABLE_TYPE,   EGL_OPENGL_ES2_BIT,
                            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
                            EGL_NONE};
  EGLConfig config;
  EGLint n = 0;
  if (!eglChooseConfig(dpy, attribs, &config, 1, &n) || n < 1)
  {
    Log("eglChooseConfig failed 0x%x (n=%d)", eglGetError(), static_cast<int>(n));
    return;
  }
  OrbisPglWindow win{0, static_cast<khronos_uint32_t>(W), static_cast<khronos_uint32_t>(H)};
  const EGLint wattr[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};
  EGLSurface surf = eglCreateWindowSurface(dpy, config, &win, wattr);
  const EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, cattr);
  if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx))
  {
    Log("surface/context failed 0x%x", eglGetError());
    return;
  }
  g_gl_ok = true;
  Log("EGL %d.%d", static_cast<int>(maj), static_cast<int>(min));
  Log("GL_VERSION:  %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  Log("GL_VENDOR:   %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
  Log("GL_RENDERER: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  Log("GLSL:        %s", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
  GLint tex = 0, vu = 0, fu = 0, tu = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &tex);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &vu);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &fu);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &tu);
  Log("max texture %d, vertex uniforms %d, fragment uniforms %d, texture units %d", tex, vu, fu, tu);

  // extension list, one per line (decides renderer options: EGLImage, NPOT, ...)
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  if (ext)
  {
    std::string e(ext);
    size_t pos = 0;
    while (pos < e.size())
    {
      size_t sp = e.find(' ', pos);
      if (sp == std::string::npos)
        sp = e.size();
      if (sp > pos)
        Log("  GL ext: %s", e.substr(pos, sp - pos).c_str());
      pos = sp + 1;
    }
  }
  const char* eext = eglQueryString(dpy, EGL_EXTENSIONS);
  Log("EGL extensions: %s", eext ? eext : "(none)");

  // ---- DECISIVE TEST: can Piglet compile GLSL at run time? (Kodi needs this;
  // OpenOrbis' sample only uses precompiled PSSL binaries via glShaderBinary.)
  {
    const char* vs = "attribute vec4 a_pos; void main(){ gl_Position = a_pos; }";
    const char* fs = "precision mediump float; uniform vec4 u_col; void main(){ gl_FragColor = u_col; }";
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);
    GLint okv = 0, okf = 0;
    glGetShaderiv(v, GL_COMPILE_STATUS, &okv);
    glGetShaderiv(f, GL_COMPILE_STATUS, &okf);
    Log("GLSL runtime compile: vertex=%s fragment=%s (glGetError 0x%x)", okv ? "OK" : "FAILED",
        okf ? "OK" : "FAILED", glGetError());
    char info[512] = {0};
    GLsizei len = 0;
    glGetShaderInfoLog(v, sizeof(info) - 1, &len, info);
    if (len) Log("  vertex log: %s", info);
    glGetShaderInfoLog(f, sizeof(info) - 1, &len, info);
    if (len) Log("  fragment log: %s", info);
    if (okv && okf)
    {
      GLuint prog = glCreateProgram();
      glAttachShader(prog, v);
      glAttachShader(prog, f);
      glBindAttribLocation(prog, 0, "a_pos");
      glLinkProgram(prog);
      GLint linked = 0;
      glGetProgramiv(prog, GL_LINK_STATUS, &linked);
      Log("GLSL runtime link: %s", linked ? "OK" : "FAILED");
    }
    GLint nfmt = 0;
    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &nfmt);
    Log("shader binary formats: %d", static_cast<int>(nfmt));
  }

  // visible test: ~3 s of colour animation (you should SEE this on the TV)
  for (int i = 0; i < 180; ++i)
  {
    float t = static_cast<float>(i) / 180.f;
    glClearColor(t, 0.2f + 0.6f * (1.f - t), 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(dpy, surf);
  }

  // YUV 4:2:0 1080p upload benchmark: what a software-decoded video costs.
  // Y plane 1920x1080 + U,V 960x540, all GL_LUMINANCE (plain GLES2).
  std::vector<unsigned char> y(static_cast<size_t>(W) * H, 128);
  std::vector<unsigned char> uv(static_cast<size_t>(W / 2) * (H / 2), 128);
  GLuint tx[3];
  glGenTextures(3, tx);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  const int sw[3] = {W, W / 2, W / 2};
  const int sh[3] = {H, H / 2, H / 2};
  for (int p = 0; p < 3; ++p)
  {
    glBindTexture(GL_TEXTURE_2D, tx[p]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, sw[p], sh[p], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 nullptr);
  }
  const int frames = 120;
  double t0 = NowMs();
  for (int i = 0; i < frames; ++i)
  {
    for (int p = 0; p < 3; ++p)
    {
      glBindTexture(GL_TEXTURE_2D, tx[p]);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sw[p], sh[p], GL_LUMINANCE, GL_UNSIGNED_BYTE,
                      p == 0 ? y.data() : uv.data());
    }
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(dpy, surf);
  }
  glFinish();
  double dt = NowMs() - t0;
  Log("YUV 1080p upload+swap: %d frames in %.1f ms = %.2f ms/frame (%.1f fps incl. vsync)", frames,
      dt, dt / frames, frames * 1000.0 / dt);
  Log("glGetError after bench: 0x%x", glGetError());

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(dpy, surf);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
}

// ------------------------------------------------------------- pad/audio
static void ProbePad()
{
  Log("== pad ==");
  int r = sceUserServiceInitialize(nullptr);
  Log("sceUserServiceInitialize: 0x%x", r);
  int32_t user = 0;
  r = sceUserServiceGetInitialUser(&user);
  Log("initial user: %d (ret 0x%x)", static_cast<int>(user), r);
  r = scePadInit();
  Log("scePadInit: 0x%x", r);
  int h = scePadOpen(user, 0, 0, nullptr);
  Log("scePadOpen: %d", h);
  if (h < 0)
    return;
  Log("Press buttons on the pad for ~8 s (changes are logged)");
  uint32_t last = 0xffffffff;
  OrbisPadData d;
  for (int i = 0; i < 480; ++i)
  {
    std::memset(&d, 0, sizeof(d));
    if (scePadReadState(h, &d) == 0 && d.buttons != last)
    {
      last = d.buttons;
      Log("  buttons=0x%08x connected=%u", d.buttons, static_cast<unsigned>(d.connected));
    }
    sceKernelUsleep(16000);
  }
  scePadClose(h);
}

static void ProbeAudio()
{
  Log("== audio ==");
  int r = sceAudioOutInit();
  Log("sceAudioOutInit: 0x%x", r);
  int32_t user = 0;
  sceUserServiceGetInitialUser(&user);
  const uint32_t grain = 1024;
  int h = sceAudioOutOpen(user, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, grain, 48000,
                          ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
  Log("sceAudioOutOpen: 0x%x", h);
  if (h < 0)
    return;
  std::vector<int16_t> buf(grain * 2);
  double ph = 0;
  for (int i = 0; i < 48000 / static_cast<int>(grain) * 2; ++i) // ~2 s of 440 Hz
  {
    for (uint32_t s = 0; s < grain; ++s)
    {
      int16_t v = static_cast<int16_t>(std::sin(ph) * 6000);
      buf[s * 2] = buf[s * 2 + 1] = v;
      ph += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
    }
    sceAudioOutOutput(h, buf.data());
  }
  sceAudioOutOutput(h, nullptr); // drain
  sceAudioOutClose(h);
  Log("audio test done (you should have heard a 2 s tone)");
}

int main()
{
  OpenLog();
  Log("PS4MC probe " __DATE__ " " __TIME__);

  // ---- Eskalacja uprawnień do root + sandbox escape ----
  {
    auto r = PS4Privileges::Escalate();
    Log("[priv] Escalate: %s  root=%s", PS4Privileges::ResultStr(r),
        PS4Privileges::IsRoot() ? "yes" : "no");
    if (r == PS4Privileges::Result::OK || r == PS4Privileges::Result::AlreadyDone)
    {
      auto m = PS4Privileges::MountDisc();
      Log("[priv] MountDisc: %s", PS4Privileges::ResultStr(m));
    }
  }

  ProbeSystem();
  ProbeDisc();
  ProbeDecoders();
  ProbePad();
  ProbeAudio();
  ProbeGL();
  Log("== done == (GL %s)", g_gl_ok ? "OK" : "FAILED");
  if (g_log)
    std::fclose(g_log);
  sceKernelSleep(5);
  return 0;
}
