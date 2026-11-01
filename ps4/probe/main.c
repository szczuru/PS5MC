/* PS4MC Probe v12
 * Cel: zdiagnozować dlaczego obraz jest czarny (dźwięk działa).
 * Zmiany względem v10/v11:
 *  - VideoOut NAJPIERW (Piglet na końcu – może zabierać display)
 *  - dłuższe czasy wyświetlania (biały ~5 s, gradient ~8 s, kolory)
 *  - ORBIS_VIDEO_OUT_FLIP_VSYNC + czekanie na flip
 *  - kilka formatów pikseli i trybów tilingu
 *  - test 720p najpierw, potem 1080p
 *  - pełne logowanie kodów błędów
 *  - fill bufora PRZED submit
 */

#include <stdbool.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/SystemService.h>
#include <orbis/VideoOut.h>
#include <orbis/Pigletv2VSH.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------ log */
static int g_logfd = -1;

static void ps4log(const char *fmt, ...)
{
    char b[768];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    size_t n = strlen(b);
    if (g_logfd >= 0) {
        write(g_logfd, b, n);
        write(g_logfd, "\n", 1);
    }
    write(1, b, n);
    write(1, "\n", 1);
}

static void open_log(void)
{
    mkdir("/data/ps4mc", 0777);
    const char *paths[] = {
        "/data/ps4mc/probe.log",
        "/mnt/usb0/ps4mc-probe.log",
        "/tmp/ps4mc-probe.log",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        g_logfd = open(paths[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (g_logfd >= 0) {
            ps4log("LOG: %s", paths[i]);
            return;
        }
    }
    ps4log("LOG: failed to open any log file (errno=%d)", errno);
}

static void ps4_exit(void)
{
    if (g_logfd >= 0) {
        close(g_logfd);
        g_logfd = -1;
    }
    sceSystemServiceLoadExec("exit", NULL);
    for (;;)
        sceKernelSleep(1);
}

/* ------------------------------------------------------------------ helpers */
static void fill_solid(uint32_t *px, int w, int h, uint32_t color)
{
    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++)
        px[i] = color;
}

static void fill_gradient(uint32_t *px, int w, int h, float t)
{
    /* A8B8G8R8: A=FF B=blue G=40 R=red */
    uint8_t red  = (uint8_t)((1.f - t) * 255);
    uint8_t blue = (uint8_t)(t * 255);
    uint32_t col = 0xFF000000u | ((uint32_t)blue << 16) | (0x40u << 8) | (uint32_t)red;
    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++)
        px[i] = col;

    /* biały prostokąt na środku */
    int x0 = w / 2 - 200, x1 = w / 2 + 200;
    int y0 = h / 2 - 60,  y1 = h / 2 + 60;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (x >= 0 && x < w && y >= 0 && y < h)
                px[y * w + x] = 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ VideoOut */
#define MAX_BUFFERS 3

static int try_videoout(int width, int height, int pixel_format, int tiling,
                        const char *tag)
{
    ps4log("== VideoOut try %s (%dx%d fmt=0x%x tiling=%d) ==",
           tag, width, height, pixel_format, tiling);

    int32_t user = 0;
    sceUserServiceGetInitialUser(&user);

    /* busType 0 = MAIN */
    int vo = sceVideoOutOpen(user, 0 /* ORBIS_VIDEO_OUT_BUS_MAIN */, 0, NULL);
    ps4log("  sceVideoOutOpen: %d (0x%x)", vo, vo);
    if (vo < 0)
        return -1;

    int r = sceVideoOutSetFlipRate(vo, 0);
    ps4log("  SetFlipRate(0): 0x%x", r);

    size_t fb_size = (size_t)width * (size_t)height * 4;
    /* 2 MiB align for GPU safety */
    const size_t align = 2 * 1024 * 1024;
    size_t total = ((fb_size * MAX_BUFFERS) + align - 1) & ~(align - 1);

    void *vaddr = NULL;
    off_t phys = 0;
    int used_direct = 0;

    r = (int)sceKernelAllocateDirectMemory(0, (off_t)0x200000000LL,
                                           total, align, 3 /* WB */, &phys);
    ps4log("  AllocateDirectMemory: 0x%x phys=0x%lx size=0x%zx",
           r, (unsigned long)phys, total);

    if (r == 0) {
        r = (int)sceKernelMapDirectMemory(&vaddr, total, 0x33 /* RW */, 0,
                                          phys, align);
        ps4log("  MapDirectMemory: 0x%x vaddr=%p", r, vaddr);
        if (r == 0 && vaddr)
            used_direct = 1;
    }

    if (!used_direct) {
        void *raw = malloc(total + align);
        if (!raw) {
            ps4log("  malloc FAIL");
            sceVideoOutClose(vo);
            return -2;
        }
        vaddr = (void *)(((uintptr_t)raw + align - 1) & ~(uintptr_t)(align - 1));
        ps4log("  malloc fallback vaddr=%p (may be invisible to GPU)", vaddr);
    }

    void *fb[MAX_BUFFERS];
    for (int i = 0; i < MAX_BUFFERS; i++)
        fb[i] = (uint8_t *)vaddr + (size_t)i * fb_size;

    OrbisVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr,
                                  pixel_format,
                                  tiling,               /* 0=tiled, 1=linear */
                                  0 /* 16:9 */,
                                  width, height, width /* pitch */);

    r = sceVideoOutRegisterBuffers(vo, 0, (void *const *)fb, MAX_BUFFERS, &attr);
    ps4log("  RegisterBuffers: 0x%x", r);
    if (r < 0) {
        sceVideoOutClose(vo);
        return -3;
    }

    /* ---- faza 1: pełna biel (ok. 5 s) ---- */
    ps4log("  faza 1: BIAŁY ekran (~5 s) – patrz na TV!");
    for (int frame = 0; frame < 300; frame++) {
        int idx = frame % MAX_BUFFERS;
        fill_solid((uint32_t *)fb[idx], width, height, 0xFFFFFFFFu);
        /* FLIP_VSYNC = 1 */
        int fr = sceVideoOutSubmitFlip(vo, idx, 1 /* VSYNC */, (int64_t)frame);
        if (frame < 3 || frame % 60 == 0)
            ps4log("    submit[%d] idx=%d -> 0x%x", frame, idx, fr);
        /* proste czekanie na flip */
        for (int w = 0; w < 30; w++) {
            if (sceVideoOutIsFlipPending(vo) == 0)
                break;
            sceKernelUsleep(1000);
        }
        sceKernelUsleep(5000);
    }

    /* ---- faza 2: gradient + prostokąt (ok. 8 s) ---- */
    ps4log("  faza 2: gradient czerwony->niebieski + biały prostokąt (~8 s)");
    for (int frame = 0; frame < 480; frame++) {
        int idx = frame % MAX_BUFFERS;
        float t = (float)frame / 480.f;
        fill_gradient((uint32_t *)fb[idx], width, height, t);
        sceVideoOutSubmitFlip(vo, idx, 1, (int64_t)(300 + frame));
        for (int w = 0; w < 30; w++) {
            if (sceVideoOutIsFlipPending(vo) == 0)
                break;
            sceKernelUsleep(1000);
        }
        sceKernelUsleep(5000);
    }

    /* ---- faza 3: kolory solidne ---- */
    ps4log("  faza 3: R / G / B / Y solid (po ~2 s każdy)");
    const uint32_t colors[] = {
        0xFF0000FFu, /* czerwony  (A8B8G8R8) */
        0xFF00FF00u, /* zielony */
        0xFFFF0000u, /* niebieski */
        0xFF00FFFFu, /* żółty */
    };
    for (int c = 0; c < 4; c++) {
        for (int frame = 0; frame < 120; frame++) {
            int idx = frame % MAX_BUFFERS;
            fill_solid((uint32_t *)fb[idx], width, height, colors[c]);
            sceVideoOutSubmitFlip(vo, idx, 1, (int64_t)(1000 + c * 120 + frame));
            sceKernelUsleep(16000);
        }
    }

    ps4log("  VideoOut %s DONE – czy coś było widać?", tag);
    sceVideoOutClose(vo);
    return 0;
}

static void probe_videoout(void)
{
    ps4log("== VideoOut suite ==");

    /* Najpierw 720p linear A8B8G8R8_SRGB – najpewniejszy */
    if (try_videoout(1280, 720,
                     0x80002200 /* A8B8G8R8_SRGB */,
                     1 /* LINEAR */,
                     "720p-linear-A8B8G8R8") == 0)
        return;

    /* 1080p linear */
    if (try_videoout(1920, 1080,
                     0x80002200,
                     1,
                     "1080p-linear-A8B8G8R8") == 0)
        return;

    /* alternatywny format A8R8G8B8_SRGB */
    if (try_videoout(1280, 720,
                     0x80000000 /* A8R8G8B8_SRGB */,
                     1,
                     "720p-linear-A8R8G8B8") == 0)
        return;

    /* tiled (0) – czasem wymagany */
    try_videoout(1280, 720,
                 0x80002200,
                 0 /* TILED */,
                 "720p-tiled-A8B8G8R8");
}

/* ------------------------------------------------------------------ Piglet */
static void probe_piglet(void)
{
    ps4log("== Piglet ==");

    const char *sw = sceKernelGetFsSandboxRandomWord();
    char path[160];
    snprintf(path, sizeof(path), "/%s/common/lib/libScePigletv2VSH.sprx",
             sw ? sw : "");
    int ms = 0;
    uint32_t h = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &ms);
    ps4log("  loadStartModule: 0x%08x (0x80020008=already, 0x8d≈OK)", h);
    if ((int32_t)h < 0 && h != 0x80020008u /* already loaded */) {
        ps4log("  Piglet load failed – pomijam");
        return;
    }

    snprintf(path, sizeof(path), "/%s/common/lib/libScePrecompiledShaders.sprx",
             sw ? sw : "");
    uint32_t shader_h = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &ms);
    ps4log("  load PrecompiledShaders: 0x%08x", shader_h);
    if ((int32_t)shader_h < 0 && shader_h != 0x80020008u) {
        ps4log("  PrecompiledShaders load failed – przerywam Piglet init");
        return;
    }

    OrbisPglConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.size = sizeof(cfg);
    cfg.processOrder = 1;
    cfg.flags = ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT |
                ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60;
    cfg.systemSharedMemorySize  = 128ull * 1024 * 1024;
    cfg.videoSharedMemorySize   = 256ull * 1024 * 1024;
    cfg.maxMappedFlexibleMemory =  64ull * 1024 * 1024;
    cfg.drawCommandBufferSize   =   1ull * 1024 * 1024;
    cfg.lcueResourceBufferSize  =   1ull * 1024 * 1024;
    cfg.dbgPosCmd_0x40 = 1920;
    cfg.dbgPosCmd_0x44 = 1080;
    cfg.unk_0x5C = 2;
    int cfg_result = scePigletSetConfigurationVSH(&cfg);
    ps4log("  scePigletSetConfigurationVSH: 0x%08x",
           (uint32_t)cfg_result);
    if (cfg_result < 0)
        return;

    EGLDisplay dpy = eglGetDisplay(0);
    ps4log("  eglGetDisplay(0): %s",
           (dpy != EGL_NO_DISPLAY) ? "OK" : "FAIL");
    if (dpy == EGL_NO_DISPLAY)
        return;

    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) {
        ps4log("  eglInitialize FAIL");
        return;
    }
    ps4log("  EGL %d.%d  GL_VENDOR=%s", major, minor,
           glGetString(GL_VENDOR) ? (const char *)glGetString(GL_VENDOR) : "?");
    ps4log("  GL_RENDERER=%s",
           glGetString(GL_RENDERER) ? (const char *)glGetString(GL_RENDERER) : "?");
    ps4log("  GL_VERSION=%s",
           glGetString(GL_VERSION) ? (const char *)glGetString(GL_VERSION) : "?");

    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint attribs[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };
    EGLConfig eglcfg;
    EGLint ncfg = 0;
    eglChooseConfig(dpy, attribs, &eglcfg, 1, &ncfg);
    ps4log("  eglChooseConfig: n=%d", ncfg);
    if (ncfg <= 0)
        return;

    OrbisPglWindow win = {0, 1920, 1080, 0};
    const EGLint surf_attribs[] = {EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE};
    EGLSurface surf = eglCreateWindowSurface(dpy, eglcfg, &win, surf_attribs);
    ps4log("  eglCreateWindowSurface: %s",
           (surf != EGL_NO_SURFACE) ? "OK" : "FAIL");
    if (surf == EGL_NO_SURFACE)
        return;

    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, eglcfg, EGL_NO_CONTEXT, ctx_attribs);
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        ps4log("  eglMakeCurrent FAIL");
        return;
    }

    /* GLSL compile test */
    const char *vs =
        "attribute vec4 p;\nvoid main(){ gl_Position = p; }\n";
    const char *fs =
        "precision mediump float;\nvoid main(){ gl_FragColor = vec4(0.0,0.6,1.0,1.0); }\n";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vsh, 1, &vs, NULL);
    glCompileShader(vsh);
    glShaderSource(fsh, 1, &fs, NULL);
    glCompileShader(fsh);
    GLint okv = 0, okf = 0;
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &okv);
    glGetShaderiv(fsh, GL_COMPILE_STATUS, &okf);
    ps4log("  GLSL compile: VS=%s FS=%s", okv ? "OK" : "FAIL", okf ? "OK" : "FAIL");

    /* prosta animacja koloru (ok. 3 s) */
    ps4log("  Piglet clear animacja (~3 s)...");
    for (int i = 0; i < 180; i++) {
        float t = (float)i / 180.f;
        glClearColor(t, 0.25f, 1.f - t, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(dpy, surf);
        sceKernelUsleep(16000);
    }
    ps4log("  Piglet animacja zakończona");
}

/* ------------------------------------------------------------------ pad / audio */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user = 0;
    sceUserServiceGetInitialUser(&user);
    scePadInit();
    int h = scePadOpen(user, 0, 0, NULL);
    ps4log("  scePadOpen: 0x%x", h);
    if (h < 0)
        return;
    OrbisPadData d;
    unsigned last = 0xffffffffu;
    for (int i = 0; i < 180; i++) {
        memset(&d, 0, sizeof(d));
        scePadReadState(h, &d);
        if (d.buttons && d.buttons != last) {
            last = d.buttons;
            ps4log("  buttons=0x%08x", (unsigned)d.buttons);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

static void probe_audio(void)
{
    ps4log("== audio ==");
    sceAudioOutInit();
    int32_t user = 0;
    sceUserServiceGetInitialUser(&user);
    int h = sceAudioOutOpen(user, 2 /* PERSONAL */, 0, 512, 48000,
                            ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    ps4log("  sceAudioOutOpen(PERSONAL): 0x%x", h);
    if (h < 0)
        return;
    short buf[512 * 2];
    double ph = 0.0;
    for (int f = 0; f < 48000 / 512; f++) {
        for (int s = 0; s < 512; s++) {
            short v = (short)(sin(ph) * 8000.0);
            buf[s * 2] = buf[s * 2 + 1] = v;
            ph += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
        }
        sceAudioOutOutput(h, buf);
    }
    sceAudioOutOutput(h, NULL);
    sceAudioOutClose(h);
    ps4log("  audio OK (440 Hz ~1 s)");
}

/* ------------------------------------------------------------------ main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v12 " __DATE__ " " __TIME__);
    ps4log("Cel: naprawić czarny ekran (dźwięk już działa)");

    sceUserServiceInitialize(NULL);

    /* VideoOut NAJPIERW – Piglet może zablokować display */
    probe_videoout();
    probe_audio();
    probe_pad();
    probe_piglet();

    ps4log("DONE – wyślij cały /data/ps4mc/probe.log");
    ps4_exit();
    return 0;
}
