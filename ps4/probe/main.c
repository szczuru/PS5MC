/* PS4MC Probe v5
   - BRAK jbc_jailbreak (padał na FW 9.00 + GoldHEN)
   - sprawdzamy co widzi aplikacja BEZ eskalacji
   - test sceKernelLoadStartModule dla Pigleta (kilka wariantów ścieżki)
   - jeśli Piglet się załaduje → test EGL + GLSL
   - test padu i audio                                                 */

#include <stdbool.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/SystemService.h>
#include <orbis/Pigletv2VSH.h>   /* EGL + GLES2 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------------------------------------------------------------- log */
static int g_logfd = -1;
static void ps4log(const char *fmt, ...)
{
    char b[512]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof(b),fmt,a); va_end(a);
    size_t n = strlen(b);
    if (g_logfd>=0){ write(g_logfd,b,n); write(g_logfd,"\n",1); }
    write(1,b,n); write(1,"\n",1);
}
static void open_log(void)
{
    mkdir("/data/ps4mc",0777);
    const char *p[]=
        {"/data/ps4mc/probe.log","/mnt/usb0/ps4mc-probe.log","/tmp/ps4mc-probe.log",NULL};
    for(int i=0;p[i];i++){
        g_logfd=open(p[i],O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(g_logfd>=0){ ps4log("LOG: %s",p[i]); return; }
    }
}
static void ps4_exit(void)
{
    if(g_logfd>=0){ close(g_logfd); g_logfd=-1; }
    sceSystemServiceLoadExec("exit",NULL);
    for(;;) sceKernelSleep(1);
}

/* ---------------------------------------------------------------- stat */
static void chk(const char *path)
{
    struct stat st; int e=stat(path,&st);
    ps4log("  stat %-36s %s",path,e==0?"OK":strerror(errno));
}

/* ---------------------------------------------------------------- system */
static void probe_system(void)
{
    ps4log("== system ==");
    ps4log("  uid=%d euid=%d pid=%d",(int)getuid(),(int)geteuid(),(int)getpid());

    /* Katalogi dostępne bez eskalacji */
    const char *dirs[]={
        "/data","/app0","/dev",
        "/mnt/disc","/mnt/usb0","/mnt/usb1",
        "/system","/system/common/lib",
        /* ścieżki które GoldHEN może udostępniać */
        "/mnt/sandbox","/hostapp","/host",
        NULL};
    for(int i=0;dirs[i];i++) chk(dirs[i]);

    /* Próba odczytu /mnt/disc bez eskalacji */
    int fd=open("/mnt/disc",O_RDONLY);
    if(fd>=0){
        unsigned char sec[2048];
        off_t o=lseek(fd,(off_t)16*2048,SEEK_SET);
        ssize_t n=read(fd,sec,sizeof(sec));
        if(o>=0&&n==2048)
            ps4log("  /mnt/disc sektor16: type=%u id='%.5s'",sec[0],(char*)&sec[1]);
        else
            ps4log("  /mnt/disc open OK lecz odczyt: %zd (%s)",n,strerror(errno));
        close(fd);
    }
}

/* ---------------------------------------------------------------- Piglet */
static int g_piglet_ok = 0;
static EGLDisplay g_dpy  = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx  = EGL_NO_CONTEXT;

static uint32_t try_load(const char *path)
{
    int ms=0;
    uint32_t h=sceKernelLoadStartModule(path,0,NULL,0,NULL,&ms);
    ps4log("  loadStartModule(%s): 0x%08x",path,h);
    return h;
}

static void probe_piglet(void)
{
    ps4log("== Piglet / EGL ==");

    /* Warianty ścieżki – GoldHEN może podmapować /system różnie */
    const char *paths[]={
        /* Przez sandbox word (sceKernelGetFsSandboxRandomWord) – spróbujemy oba */
        "/system/common/lib/libScePigletv2VSH.sprx",
        "/system_ex/common_ex/lib/libScePigletv2VSH.sprx",
        /* GoldHEN typowo udostępnia przez /hostapp */
        "/hostapp/system/common/lib/libScePigletv2VSH.sprx",
        /* Względem /app0 (jeśli paczka ma moduł) */
        "/app0/sce_module/libScePigletv2VSH.sprx",
        NULL
    };

    uint32_t piglet=0xffffffff;
    for(int i=0;paths[i];i++){
        piglet=try_load(paths[i]);
        if((int32_t)piglet>=0) break;
    }
    if((int32_t)piglet<0){ ps4log("  Piglet: nie zaladowany"); return; }

    /* libScePrecompiledShaders – opcjonalna */
    const char *shpaths[]={
        "/system/common/lib/libScePrecompiledShaders.sprx",
        "/system_ex/common_ex/lib/libScePrecompiledShaders.sprx",
        NULL
    };
    for(int i=0;shpaths[i];i++){
        uint32_t h=try_load(shpaths[i]);
        if((int32_t)h>=0) break;
    }

    /* Konfiguracja Pigleta */
    const int W=1920,H=1080;
    OrbisPglConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT|ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY|0x60;
    cfg.processOrder=1;
    cfg.systemSharedMemorySize  =250ull*1024*1024;
    cfg.videoSharedMemorySize   =512ull*1024*1024;
    cfg.maxMappedFlexibleMemory =170ull*1024*1024;
    cfg.drawCommandBufferSize   =  1  *1024*1024;
    cfg.lcueResourceBufferSize  =  1  *1024*1024;
    cfg.dbgPosCmd_0x40=W; cfg.dbgPosCmd_0x44=H;
    cfg.unk_0x5C=2;

    if(!scePigletSetConfigurationVSH(&cfg)){
        ps4log("  scePigletSetConfigurationVSH: FAIL"); return;
    }
    ps4log("  scePigletSetConfigurationVSH: OK");

    g_dpy=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj=0,min=0;
    if(g_dpy==EGL_NO_DISPLAY||!eglInitialize(g_dpy,&maj,&min)){
        ps4log("  eglInitialize: FAIL 0x%x",eglGetError()); return;
    }
    ps4log("  EGL %d.%d",maj,min);
    ps4log("  GL_VERSION:  %s",glGetString(GL_VERSION));
    ps4log("  GL_RENDERER: %s",glGetString(GL_RENDERER));

    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint att[]={
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
    EGLConfig cfg2; EGLint n=0;
    if(!eglChooseConfig(g_dpy,att,&cfg2,1,&n)||n<1){
        ps4log("  eglChooseConfig: FAIL 0x%x",eglGetError()); return;
    }
    OrbisPglWindow win={0,(khronos_uint32_t)W,(khronos_uint32_t)H};
    const EGLint wa[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
    g_surf=eglCreateWindowSurface(g_dpy,cfg2,&win,wa);
    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    g_ctx=eglCreateContext(g_dpy,cfg2,EGL_NO_CONTEXT,ca);
    if(g_surf==EGL_NO_SURFACE||g_ctx==EGL_NO_CONTEXT||
       !eglMakeCurrent(g_dpy,g_surf,g_surf,g_ctx)){
        ps4log("  eglMakeCurrent: FAIL 0x%x",eglGetError()); return;
    }
    g_piglet_ok=1;
    ps4log("  eglMakeCurrent: OK");

    /* Test kompilacji GLSL – KLUCZOWE dla Kodi */
    const char *vs="attribute vec4 p; void main(){gl_Position=p;}";
    const char *fs="precision mediump float; void main(){gl_FragColor=vec4(1.0);}";
    GLuint v=glCreateShader(GL_VERTEX_SHADER);
    GLuint f=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v,1,&vs,NULL); glCompileShader(v);
    glShaderSource(f,1,&fs,NULL); glCompileShader(f);
    GLint ov=0,of=0;
    glGetShaderiv(v,GL_COMPILE_STATUS,&ov);
    glGetShaderiv(f,GL_COMPILE_STATUS,&of);
    ps4log("  GLSL compile: vertex=%s fragment=%s (err 0x%x)",
           ov?"OK":"FAIL", of?"OK":"FAIL", glGetError());
    char info[256]={0}; GLsizei len=0;
    if(!ov){ glGetShaderInfoLog(v,sizeof(info)-1,&len,info); ps4log("  VS log: %s",info); }
    if(!of){ glGetShaderInfoLog(f,sizeof(info)-1,&len,info); ps4log("  FS log: %s",info); }

    /* Animacja ~2 s (widoczna na TV – potwierdzenie że GPU działa) */
    ps4log("  animacja 2 s na ekranie...");
    for(int i=0;i<120;i++){
        float t=(float)i/120.f;
        glClearColor(t,0.3f*(1.f-t),0.8f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(g_dpy,g_surf);
    }
    ps4log("  animacja OK, glGetError=0x%x",glGetError());

    eglMakeCurrent(g_dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(g_dpy,g_surf);
    eglDestroyContext(g_dpy,g_ctx);
    eglTerminate(g_dpy);
}

/* ---------------------------------------------------------------- dekoderowe moduły */
static void probe_decoders(void)
{
    ps4log("== moduły dekoderów ==");
    const char *mods[]={
        "/system/common/lib/libSceVideodec.sprx",
        "/system/common/lib/libSceVideodec2.sprx",
        "/system/common/lib/libSceVdecCore.sprx",
        "/system/common/lib/libSceAvPlayer.sprx",
        NULL
    };
    for(int i=0;mods[i];i++) try_load(mods[i]);
}

/* ---------------------------------------------------------------- pad */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    ps4log("  user: %d",user);
    scePadInit();
    int h=scePadOpen(user,0,0,NULL);
    ps4log("  scePadOpen: 0x%x",h);
    if(h<0) return;
    ps4log("  nacisnic przycisk (2 s)...");
    OrbisPadData d; unsigned last=0xffffffff;
    for(int i=0;i<120;i++){
        memset(&d,0,sizeof(d));
        scePadReadState(h,&d);
        if(d.buttons&&d.buttons!=last){
            last=d.buttons;
            ps4log("  buttons=0x%08x",d.buttons);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

/* ---------------------------------------------------------------- audio */
static void probe_audio(void)
{
    ps4log("== audio ==");
    int r=sceAudioOutInit();
    ps4log("  sceAudioOutInit: 0x%x",r);
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    int h=sceAudioOutOpen(user,ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,
                          512,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    ps4log("  sceAudioOutOpen: 0x%x",h);
    if(h<0) return;
    short buf[512*2]; double ph=0.0;
    for(int f=0;f<48000/512;f++){
        for(int s=0;s<512;s++){
            short v=(short)(sin(ph)*8000.0);
            buf[s*2]=buf[s*2+1]=v;
            ph+=2.0*3.14159265358979*440.0/48000.0;
        }
        sceAudioOutOutput(h,buf);
    }
    sceAudioOutOutput(h,NULL);
    sceAudioOutClose(h);
    ps4log("  audio OK");
}

/* ---------------------------------------------------------------- main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v5 " __DATE__ " " __TIME__);
    sceUserServiceInitialize(NULL);

    probe_system();
    probe_piglet();
    probe_decoders();
    probe_pad();
    probe_audio();

    ps4log("DONE");
    ps4_exit();
    return 0;
}
