/* PS4MC Probe v7
   - listowanie /<sandboxword>/common/lib/ (znajdz shader compiler)
   - sceVideoOutOpen przed EGL
   - 4 warianty konfiguracji Pigleta (rozne flagi/pamiec dla PS4 GCN)
   - sprawdzenie dostepnej pamieci flexible
   - audio przez VOICE port                                           */

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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_logfd = -1;
static void ps4log(const char *fmt, ...)
{
    char b[512]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof(b),fmt,a); va_end(a);
    size_t n=strlen(b);
    if(g_logfd>=0){ write(g_logfd,b,n); write(g_logfd,"\n",1); }
    write(1,b,n); write(1,"\n",1);
}
static void open_log(void)
{
    mkdir("/data/ps4mc",0777);
    const char *p[]={"/data/ps4mc/probe.log","/mnt/usb0/ps4mc-probe.log",
                     "/tmp/ps4mc-probe.log",NULL};
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
static uint32_t try_load(const char *path)
{
    int ms=0;
    uint32_t h=sceKernelLoadStartModule(path,0,NULL,0,NULL,&ms);
    ps4log("  load 0x%08x  %s",h,path);
    return h;
}

/* --------------------------------------------------------------- system */
static void probe_system(void)
{
    ps4log("== system ==");
    const char *sw=sceKernelGetFsSandboxRandomWord();
    ps4log("  sandboxWord: [%s]",sw?sw:"null");

    /* Dostepna pamiec flexible (limituje konfiguracje Pigleta) */
    size_t flex=0;
    { typedef int(*fn_t)(size_t*); fn_t fn=(fn_t)(void*)sceKernelAvailableFlexibleMemorySize; fn(&flex); }
    ps4log("  flexible memory: %zu MB",flex/(1024*1024));

    /* Listuj /<sw>/common/lib/ aby znalezc shader compiler */
    if(sw&&sw[0]){
        char libpath[128];
        snprintf(libpath,sizeof(libpath),"/%s/common/lib",sw);
        DIR *d=opendir(libpath);
        if(d){
            ps4log("  ls %s:",libpath);
            struct dirent *e; int n=0;
            while((e=readdir(d))&&n<60){
                if(strstr(e->d_name,".sprx")){
                    ps4log("    %s",e->d_name);
                    n++;
                }
            }
            closedir(d);
        } else {
            ps4log("  opendir(%s): %s",libpath,strerror(errno));
        }
    }
}

/* --------------------------------------------------------------- VideoOut */
static int g_voHandle = -1;

static void open_video_out(void)
{
    ps4log("== VideoOut ==");
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    /* BUS_TYPE_MAIN=0, index=0 */
    /* Na PS4 VideoOut wymaga user=0xFF (system) aby zwrocil maly handle */
    g_voHandle=sceVideoOutOpen(0xFF,0,0,NULL);
    ps4log("  sceVideoOutOpen(0xFF): %d (0x%x)",g_voHandle,g_voHandle);
    if(g_voHandle<0){
        g_voHandle=sceVideoOutOpen(user,0,0,NULL);
        ps4log("  sceVideoOutOpen(user=%d): %d (0x%x)",user,g_voHandle,g_voHandle);
    }
}

/* --------------------------------------------------------------- Piglet */
static int try_egl(const char *label, OrbisPglConfig *cfg)
{
    ps4log("  --- [%s] ---",label);
    if(!scePigletSetConfigurationVSH(cfg)){
        ps4log("  scePigletSetConfigurationVSH: FAIL");
        return 0;
    }
    ps4log("  scePigletSetConfigurationVSH: OK");

    /* PS4 Piglet: eglGetDisplay potrzebuje VideoOut handle, nie EGL_DEFAULT_DISPLAY.
       eglGetDisplay przyjmuje EGLNativeDisplayType=int, wiec rzutujemy przez int. */
    typedef EGLDisplay (*pfnGetDisplay)(int);
    pfnGetDisplay fnGetDisp=(pfnGetDisplay)(void*)eglGetDisplay;
    EGLDisplay dpy=EGL_NO_DISPLAY;
    int displays[]={g_voHandle,1,2,3,0};
    for(int di=0;di<5&&dpy==EGL_NO_DISPLAY;di++){
        dpy=fnGetDisp(displays[di]);
        ps4log("  eglGetDisplay(%d): %p",displays[di],(void*)dpy);
    }
    if(dpy==EGL_NO_DISPLAY){
        ps4log("  wszystkie getDisplay: NO_DISPLAY (err 0x%x)",eglGetError());
        return 0;
    }
    EGLint maj=0,mn=0;
    if(!eglInitialize(dpy,&maj,&mn)){
        ps4log("  eglInitialize: FAIL err=0x%x",eglGetError());
        eglTerminate(dpy);
        return 0;
    }
    ps4log("  eglInitialize: OK %d.%d",maj,mn);
    ps4log("  GL_VERSION:  %s",glGetString(GL_VERSION));
    ps4log("  GL_RENDERER: %s",glGetString(GL_RENDERER));

    /* GLSL compile test */
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint att[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_ALPHA_SIZE,8,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
    EGLConfig ec; EGLint n=0;
    if(!eglChooseConfig(dpy,att,&ec,1,&n)||n<1){
        ps4log("  eglChooseConfig: FAIL 0x%x",eglGetError());
        eglTerminate(dpy); return 0;
    }
    OrbisPglWindow win={0,1920,1080,0};
    const EGLint wa[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
    EGLSurface surf=eglCreateWindowSurface(dpy,ec,&win,wa);
    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext ctx=eglCreateContext(dpy,ec,EGL_NO_CONTEXT,ca);
    if(surf==EGL_NO_SURFACE||ctx==EGL_NO_CONTEXT||
       !eglMakeCurrent(dpy,surf,surf,ctx)){
        ps4log("  eglMakeCurrent: FAIL 0x%x",eglGetError());
        eglDestroyContext(dpy,ctx); eglDestroySurface(dpy,surf);
        eglTerminate(dpy); return 0;
    }
    ps4log("  eglMakeCurrent: OK");

    /* GLSL */
    const char *vs="attribute vec4 p; void main(){gl_Position=p;}";
    const char *fs="precision mediump float; void main(){gl_FragColor=vec4(1.0);}";
    GLuint v=glCreateShader(GL_VERTEX_SHADER),f=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v,1,&vs,NULL); glCompileShader(v);
    glShaderSource(f,1,&fs,NULL); glCompileShader(f);
    GLint ov=0,of=0;
    glGetShaderiv(v,GL_COMPILE_STATUS,&ov);
    glGetShaderiv(f,GL_COMPILE_STATUS,&of);
    ps4log("  GLSL compile: VS=%s FS=%s err=0x%x",
           ov?"OK":"FAIL",of?"OK":"FAIL",glGetError());
    if(!ov||!of){
        char info[256]={0}; GLsizei len=0;
        if(!ov){glGetShaderInfoLog(v,255,&len,info);ps4log("  VS: %s",info);}
        if(!of){glGetShaderInfoLog(f,255,&len,info);ps4log("  FS: %s",info);}
    }

    /* Animacja 2s */
    for(int i=0;i<120;i++){
        float t=(float)i/120.f;
        glClearColor(t,0.3f*(1.f-t),0.8f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(dpy,surf);
    }
    ps4log("  animacja OK – Piglet dziala!");

    eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(dpy,ctx); eglDestroySurface(dpy,surf); eglTerminate(dpy);
    return 1;
}

static void probe_piglet(void)
{
    ps4log("== Piglet / EGL ==");
    const char *sw=sceKernelGetFsSandboxRandomWord();
    if(!sw||!sw[0]){ ps4log("  brak sandbox word"); return; }

    char ppath[128],spath[128];
    snprintf(ppath,sizeof(ppath),"/%s/common/lib/libScePigletv2VSH.sprx",sw);
    snprintf(spath,sizeof(spath),"/%s/common/lib/libScePrecompiledShaders.sprx",sw);
    if((int32_t)try_load(ppath)<0){ ps4log("  Piglet nie zaladowany"); return; }
    try_load(spath);

    OrbisPglConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    cfg.processOrder=1;
    cfg.dbgPosCmd_0x40=1920; cfg.dbgPosCmd_0x44=1080;

    /* Wariant A: minimalne flagi, mala pamiec (PS4-safe) */
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT; /* 0x08 only */
    cfg.systemSharedMemorySize =  64ull*1024*1024;
    cfg.videoSharedMemorySize  = 128ull*1024*1024;
    cfg.maxMappedFlexibleMemory=   0;
    cfg.drawCommandBufferSize  =   1*1024*1024;
    cfg.lcueResourceBufferSize =   1*1024*1024;
    cfg.unk_0x5C=0;
    if(try_egl("A: composite+64MB+128MB",&cfg)) return;

    /* Wariant B: z flexible memory, mniejsza niz PS5 */
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT|ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY;
    cfg.systemSharedMemorySize =  64ull*1024*1024;
    cfg.videoSharedMemorySize  = 128ull*1024*1024;
    cfg.maxMappedFlexibleMemory=  64ull*1024*1024;
    cfg.drawCommandBufferSize  =   1*1024*1024;
    cfg.lcueResourceBufferSize =   1*1024*1024;
    cfg.unk_0x5C=2;
    if(try_egl("B: composite+flex+64MB",&cfg)) return;

    /* Wariant C: skip app init (VSH-mode) */
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT
             |ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY
             |ORBIS_PGL_FLAGS_SKIP_APP_INITIALIZATION;
    cfg.systemSharedMemorySize = 128ull*1024*1024;
    cfg.videoSharedMemorySize  = 256ull*1024*1024;
    cfg.maxMappedFlexibleMemory=  64ull*1024*1024;
    cfg.unk_0x5C=2;
    if(try_egl("C: skip_app_init+128MB",&cfg)) return;

    /* Wariant D: pelna konfiguracja PS5 (moze dzialac na PS4 pro?) */
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT
             |ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY|0x60;
    cfg.systemSharedMemorySize =250ull*1024*1024;
    cfg.videoSharedMemorySize  =512ull*1024*1024;
    cfg.maxMappedFlexibleMemory=170ull*1024*1024;
    cfg.unk_0x5C=2;
    if(try_egl("D: ps5-full",&cfg)) return;

    ps4log("  WSZYSTKIE warianty FAIL");
}

/* --------------------------------------------------------------- dekoderowe moduly */
static void probe_decoders(void)
{
    ps4log("== moduly dekoderow ==");
    struct { uint16_t id; const char *name; } mods[]={
        {0x008E,"Videodec"},{0x00CF,"Videodec2"},{0x00D0,"Vdecwrap"},
        {0x00A5,"AvPlayer"},{0x00D7,"DiscMap"},
        {0x0088,"Audiodec"},{0x008A,"JpegDec"},{0x008C,"PngDec"},
        {0x0000,NULL}
    };
    for(int i=0;mods[i].name;i++){
        int32_t r=sceSysmoduleLoadModule((enum OrbisSysModule)mods[i].id);
        ps4log("  sysmod(0x%04x %-10s) = 0x%x",mods[i].id,mods[i].name,r);
    }
}

/* --------------------------------------------------------------- pad */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    scePadInit();
    int h=scePadOpen(user,0,0,NULL);
    ps4log("  scePadOpen: 0x%x",h);
    if(h<0) return;
    OrbisPadData d; unsigned last=0xffffffff;
    for(int i=0;i<120;i++){
        memset(&d,0,sizeof(d));
        scePadReadState(h,&d);
        if(d.buttons&&d.buttons!=last){ last=d.buttons;
            ps4log("  buttons=0x%08x",(unsigned)d.buttons); }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

/* --------------------------------------------------------------- audio */
static void probe_audio(void)
{
    ps4log("== audio ==");
    /* Zaladuj AudioDec sysmodule ktory moze byc wymagany */
    sceSysmoduleLoadModule((enum OrbisSysModule)0x0088);

    int r=sceAudioOutInit();
    ps4log("  sceAudioOutInit: 0x%x",r);

    /* Probuj rozne typy portow */
    struct { int type; const char *name; } ports[]={
        {0,"MAIN"},{5,"BGM"},{6,"VOICE"},{1,"CHAT"},{2,"PERSONAL"},{-1,NULL}};
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    for(int i=0;ports[i].name;i++){
        int h=sceAudioOutOpen(user,(enum OrbisAudioOutPort)ports[i].type,
                              0,512,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        ps4log("  open(user=%d type=%s): 0x%x",user,ports[i].name,h);
        if(h>=0){
            short buf[512*2]; double ph=0.0;
            for(int f=0;f<48000/512;f++){
                for(int s=0;s<512;s++){
                    short v=(short)(sin(ph)*8000.0);
                    buf[s*2]=buf[s*2+1]=v; ph+=2.0*3.14159265358979*440.0/48000.0;
                }
                sceAudioOutOutput(h,buf);
            }
            sceAudioOutOutput(h,NULL); sceAudioOutClose(h);
            ps4log("  audio OK (type=%s)",ports[i].name);
            break;
        }
    }
}

/* --------------------------------------------------------------- main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v7 " __DATE__ " " __TIME__);
    sceUserServiceInitialize(NULL);
    probe_system();
    open_video_out();
    probe_piglet();
    probe_decoders();
    probe_pad();
    probe_audio();
    ps4log("DONE");
    ps4_exit();
    return 0;
}
