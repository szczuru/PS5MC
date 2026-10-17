/* PS4MC Probe v6
   - sceKernelGetFsSandboxRandomWord() -> wlasciwa sciezka do Pigleta
   - sceSysmoduleLoadModule() dla Videodec/Videodec2/AvPlayer (nie potrzebuja path)
   - eksploracja /hostapp i /host (GoldHEN FS)
   - poprawka audio: kilka wariantow user ID                         */

#include <stdbool.h>
#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/SystemService.h>
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
static void chk(const char *path)
{
    struct stat st; int e=stat(path,&st);
    ps4log("  stat %-40s %s",path,e==0?"OK":strerror(errno));
}
static void ls_dir(const char *path)
{
    DIR *d=opendir(path);
    if(!d){ ps4log("  ls %s: %s",path,strerror(errno)); return; }
    ps4log("  ls %s:",path);
    struct dirent *e; int n=0;
    while((e=readdir(d))&&n<20){
        if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))
            { ps4log("    %s",e->d_name); n++; }
    }
    closedir(d);
}
static uint32_t try_load(const char *path)
{
    int ms=0;
    uint32_t h=sceKernelLoadStartModule(path,0,NULL,0,NULL,&ms);
    ps4log("  loadStartModule 0x%08x  %s",h,path);
    return h;
}
static void try_sysmod(uint16_t id, const char *name)
{
    int32_t r=sceSysmoduleLoadModule((enum OrbisSysModule)id);
    ps4log("  sceSysmoduleLoadModule(0x%04x %-16s) = 0x%x",id,name,r);
}

/* --------------------------------------------------------------- system */
static void probe_system(void)
{
    ps4log("== system ==");
    ps4log("  uid=%d euid=%d pid=%d",(int)getuid(),(int)geteuid(),(int)getpid());

    const char *sw=sceKernelGetFsSandboxRandomWord();
    ps4log("  sandboxRandomWord: [%s]",sw?sw:"null");

    if(sw&&sw[0]){
        char p[256];
        snprintf(p,sizeof(p),"/%s",sw);              chk(p);
        snprintf(p,sizeof(p),"/%s/common/lib",sw);   chk(p);
        snprintf(p,sizeof(p),"/%s/common/lib/libScePigletv2VSH.sprx",sw);   chk(p);
        snprintf(p,sizeof(p),"/%s/common/lib/libSceVideodec2.sprx",sw);     chk(p);
    }
    chk("/data"); chk("/app0"); chk("/dev");
    chk("/mnt/disc"); chk("/mnt/usb0");
    chk("/hostapp"); chk("/host");
    ls_dir("/hostapp");
    ls_dir("/host");
}

/* --------------------------------------------------------------- Piglet */
static void probe_piglet(void)
{
    ps4log("== Piglet / EGL ==");
    const char *sw=sceKernelGetFsSandboxRandomWord();
    uint32_t piglet=0xffffffff;

    /* Przez sandbox word */
    if(sw&&sw[0]){
        char p[256];
        snprintf(p,sizeof(p),"/%s/common/lib/libScePigletv2VSH.sprx",sw);
        piglet=try_load(p);
        if((int32_t)piglet>=0){
            snprintf(p,sizeof(p),"/%s/common/lib/libScePrecompiledShaders.sprx",sw);
            try_load(p);
            snprintf(p,sizeof(p),"/%s/common/lib/libSceShaccVSH.sprx",sw);
            try_load(p);
        }
    }
    /* Fallback */
    if((int32_t)piglet<0){
        const char *ps[]={"/system/common/lib/libScePigletv2VSH.sprx",
                          "/hostapp/system/common/lib/libScePigletv2VSH.sprx",
                          "/host/system/common/lib/libScePigletv2VSH.sprx",NULL};
        for(int i=0;ps[i]&&(int32_t)piglet<0;i++) piglet=try_load(ps[i]);
    }
    if((int32_t)piglet<0){ ps4log("  Piglet: nie zaladowany"); return; }

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
    cfg.dbgPosCmd_0x40=W; cfg.dbgPosCmd_0x44=H; cfg.unk_0x5C=2;
    if(!scePigletSetConfigurationVSH(&cfg)){
        ps4log("  scePigletSetConfigurationVSH: FAIL"); return;
    }
    ps4log("  scePigletSetConfigurationVSH: OK");

    EGLDisplay dpy=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj=0,mn=0;
    if(dpy==EGL_NO_DISPLAY||!eglInitialize(dpy,&maj,&mn)){
        ps4log("  eglInitialize: FAIL 0x%x",eglGetError()); return;
    }
    ps4log("  EGL %d.%d  GL: %s",maj,mn,glGetString(GL_VERSION));
    ps4log("  GL_RENDERER: %s",glGetString(GL_RENDERER));

    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint att[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_ALPHA_SIZE,8,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
    EGLConfig ec; EGLint n=0;
    if(!eglChooseConfig(dpy,att,&ec,1,&n)||n<1){
        ps4log("  eglChooseConfig: FAIL 0x%x",eglGetError()); return;
    }
    OrbisPglWindow win={0,(khronos_uint32_t)W,(khronos_uint32_t)H};
    const EGLint wa[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
    EGLSurface surf=eglCreateWindowSurface(dpy,ec,&win,wa);
    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext ctx=eglCreateContext(dpy,ec,EGL_NO_CONTEXT,ca);
    if(surf==EGL_NO_SURFACE||ctx==EGL_NO_CONTEXT||
       !eglMakeCurrent(dpy,surf,surf,ctx)){
        ps4log("  eglMakeCurrent: FAIL 0x%x",eglGetError()); return;
    }
    ps4log("  eglMakeCurrent: OK");

    const char *vs="attribute vec4 p; void main(){gl_Position=p;}";
    const char *fs="precision mediump float; void main(){gl_FragColor=vec4(1.0);}";
    GLuint v=glCreateShader(GL_VERTEX_SHADER), f=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v,1,&vs,NULL); glCompileShader(v);
    glShaderSource(f,1,&fs,NULL); glCompileShader(f);
    GLint ov=0,of=0;
    glGetShaderiv(v,GL_COMPILE_STATUS,&ov);
    glGetShaderiv(f,GL_COMPILE_STATUS,&of);
    ps4log("  GLSL compile: vertex=%s fragment=%s err=0x%x",
           ov?"OK":"FAIL",of?"OK":"FAIL",glGetError());
    if(!ov||!of){
        char info[256]={0}; GLsizei len=0;
        if(!ov){ glGetShaderInfoLog(v,255,&len,info); ps4log("  VS: %s",info); }
        if(!of){ glGetShaderInfoLog(f,255,&len,info); ps4log("  FS: %s",info); }
    }
    for(int i=0;i<120;i++){
        float t=(float)i/120.f;
        glClearColor(t,0.3f*(1.f-t),0.8f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(dpy,surf);
    }
    ps4log("  animacja 2s OK");
    eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroySurface(dpy,surf); eglDestroyContext(dpy,ctx); eglTerminate(dpy);
}

/* --------------------------------------------------------------- dekoderowe moduly */
static void probe_decoders(void)
{
    ps4log("== moduly dekoderow (sceSysmoduleLoadModule) ==");
    try_sysmod(0x008E,"Videodec");
    try_sysmod(0x00CF,"Videodec2");
    try_sysmod(0x00D0,"Vdecwrap");
    try_sysmod(0x00A5,"AvPlayer");
    try_sysmod(0x00D7,"DiscMap");
    try_sysmod(0x008A,"JpegDec");
    try_sysmod(0x008C,"PngDec");
}

/* --------------------------------------------------------------- pad */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    ps4log("  initialUser=%d",user);
    scePadInit();
    int h=scePadOpen(user,0,0,NULL);
    ps4log("  scePadOpen(user=%d): 0x%x",user,h);
    if(h<0) return;
    OrbisPadData d; unsigned last=0xffffffff;
    for(int i=0;i<120;i++){
        memset(&d,0,sizeof(d));
        scePadReadState(h,&d);
        if(d.buttons&&d.buttons!=last){
            last=d.buttons;
            ps4log("  buttons=0x%08x",(unsigned)d.buttons);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

/* --------------------------------------------------------------- audio */
static void probe_audio(void)
{
    ps4log("== audio ==");
    int r=sceAudioOutInit();
    ps4log("  sceAudioOutInit: 0x%x",r);
    /* user=382545047 moze byc bledny dla audio – probuj kilka wartosci */
    const int32_t users[]={1,0,-1,382545047};
    for(int i=0;i<4;i++){
        int h=sceAudioOutOpen(users[i],ORBIS_AUDIO_OUT_PORT_TYPE_MAIN,0,
                              512,48000,ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        ps4log("  sceAudioOutOpen(user=%d): 0x%x",users[i],h);
        if(h>=0){
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
            ps4log("  audio OK (user=%d)",users[i]);
            break;
        }
    }
}

/* --------------------------------------------------------------- main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v6 " __DATE__ " " __TIME__);
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
