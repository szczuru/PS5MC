/* PS4MC Probe v9
   - PAID zmieniony na 0x3800000000000035 (SceVideoServicePlayer)
   - test Pigleta z nowym PAID (moze da dostep do kompozytora)
   - test VideoOut framebuffer (plan B - software rendering bez EGL)
   - potwierdzenie audio i padu                                      */

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

/* --------------------------------------------------------------- Piglet */
static void probe_piglet(void)
{
    ps4log("== Piglet / EGL (PAID=0x35) ==");
    const char *sw=sceKernelGetFsSandboxRandomWord();
    if(!sw||!sw[0]){ ps4log("  brak sandbox word"); return; }

    char p[128];
    snprintf(p,sizeof(p),"/%s/common/lib/libScePigletv2VSH.sprx",sw);
    int ms=0; uint32_t h=sceKernelLoadStartModule(p,0,NULL,0,NULL,&ms);
    ps4log("  Piglet load: 0x%08x",h);
    if((int32_t)h<0) return;
    snprintf(p,sizeof(p),"/%s/common/lib/libScePrecompiledShaders.sprx",sw);
    sceKernelLoadStartModule(p,0,NULL,0,NULL,&ms);

    OrbisPglConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    cfg.processOrder=1;
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT|ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY;
    cfg.systemSharedMemorySize  = 128ull*1024*1024;
    cfg.videoSharedMemorySize   = 256ull*1024*1024;
    cfg.maxMappedFlexibleMemory =  64ull*1024*1024;
    cfg.drawCommandBufferSize   =   1  *1024*1024;
    cfg.lcueResourceBufferSize  =   1  *1024*1024;
    cfg.dbgPosCmd_0x40=1920; cfg.dbgPosCmd_0x44=1080;
    cfg.unk_0x5C=2;

    if(!scePigletSetConfigurationVSH(&cfg)){
        ps4log("  scePigletSetConfigurationVSH: FAIL"); return;
    }
    ps4log("  scePigletSetConfigurationVSH: OK");

    /* Sprobuj z kilkoma wartosciami display ID */
    typedef EGLDisplay (*pfnGD)(int);
    pfnGD getDisp=(pfnGD)(void*)eglGetDisplay;
    EGLDisplay dpy=EGL_NO_DISPLAY;
    int ids[]={0,1,2,3,4,5,6,7,-1};
    for(int i=0;i<9&&dpy==EGL_NO_DISPLAY;i++){
        dpy=getDisp(ids[i]);
        if(dpy!=EGL_NO_DISPLAY)
            ps4log("  eglGetDisplay(%d): OK -> %p",ids[i],(void*)dpy);
    }
    if(dpy==EGL_NO_DISPLAY){
        ps4log("  eglGetDisplay: FAIL (err 0x%x) - PAID 0x35 nie wystarczy",
               eglGetError());
        ps4log("  -> przechodzimy na VideoOut framebuffer");
        return;
    }

    EGLint maj=0,mn=0;
    if(!eglInitialize(dpy,&maj,&mn)){
        ps4log("  eglInitialize: FAIL 0x%x",eglGetError()); return;
    }
    ps4log("  eglInitialize: OK %d.%d",maj,mn);
    ps4log("  GL: %s",glGetString(GL_VERSION));

    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint att[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_ALPHA_SIZE,8,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
    EGLConfig ec; EGLint n=0;
    eglChooseConfig(dpy,att,&ec,1,&n);
    OrbisPglWindow win={0,1920,1080,0};
    const EGLint wa[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
    EGLSurface surf=eglCreateWindowSurface(dpy,ec,&win,wa);
    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext ctx=eglCreateContext(dpy,ec,EGL_NO_CONTEXT,ca);
    if(!eglMakeCurrent(dpy,surf,surf,ctx)){
        ps4log("  eglMakeCurrent: FAIL 0x%x",eglGetError()); return;
    }
    ps4log("  EGL OK! GLSL test...");
    const char *vs="attribute vec4 p; void main(){gl_Position=p;}";
    const char *fs="precision mediump float; void main(){gl_FragColor=vec4(0.0,0.5,1.0,1.0);}";
    GLuint v=glCreateShader(GL_VERTEX_SHADER),f=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v,1,&vs,NULL); glCompileShader(v);
    glShaderSource(f,1,&fs,NULL); glCompileShader(f);
    GLint ov=0,of=0;
    glGetShaderiv(v,GL_COMPILE_STATUS,&ov);
    glGetShaderiv(f,GL_COMPILE_STATUS,&of);
    ps4log("  GLSL: VS=%s FS=%s",ov?"OK":"FAIL",of?"OK":"FAIL");
    for(int i=0;i<120;i++){
        float t=(float)i/120.f;
        glClearColor(0.f,t*0.5f,1.f-t,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(dpy,surf);
    }
    ps4log("  animacja OK – PIGLET DZIALA!");
    eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(dpy,ctx); eglDestroySurface(dpy,surf); eglTerminate(dpy);
}

/* --------------------------------------------------------------- VideoOut framebuffer */
/* Plan B: VideoOut + software rendering bez EGL.
   Alokujemy 3 bufory A8B8G8R8_SRGB (triple buffering) przez
   sceKernelAllocateDirectMemory, rejestrujemy w VideoOut,
   rysujemy CPU i wysylamy przez sceVideoOutSubmitFlip.            */

#define FB_W 1920
#define FB_H 1080
#define FB_SIZE ((size_t)FB_W * FB_H * 4)
#define FB_ALIGN (2*1024*1024)  /* VideoOut wymaga 2 MB alignment */

static void probe_videoout_fb(void)
{
    ps4log("== VideoOut framebuffer (plan B bez EGL) ==");

    int32_t user=0; sceUserServiceGetInitialUser(&user);
    int vo=sceVideoOutOpen(user,0,0,NULL);
    ps4log("  sceVideoOutOpen(user=%d): %d",user,vo);
    if(vo<0){ ps4log("  FAIL"); return; }

    /* Alokuj pamiec Direct (GPU-visible) przez kernel */
    const size_t total=FB_SIZE*3;
    const size_t aligned=((total+FB_ALIGN-1)/FB_ALIGN)*FB_ALIGN;
    off_t phys=0;
    int r=(int)sceKernelAllocateDirectMemory(0,(off_t)0x200000000LL,
        (size_t)aligned,(size_t)FB_ALIGN,3,&phys);
    ps4log("  AllocateDirectMemory(%zu MB): 0x%x phys=0x%lx",
           aligned/(1024*1024),r,(unsigned long)phys);

    void *vaddr=NULL;
    if(r==0){
        r=(int)sceKernelMapDirectMemory(&vaddr,(size_t)aligned,
            0x33 /*PROT_CPU_READ|PROT_CPU_WRITE|PROT_GPU_ALL*/,
            0,phys,(size_t)FB_ALIGN);
        ps4log("  MapDirectMemory: 0x%x vaddr=%p",r,vaddr);
    }
    if(r!=0||!vaddr){
        ps4log("  pamiec direct FAIL -> malloc fallback");
        vaddr=malloc(total+FB_ALIGN);
        if(!vaddr){ ps4log("  malloc FAIL"); return; }
        vaddr=(void*)(((uintptr_t)vaddr+FB_ALIGN-1)&~(uintptr_t)(FB_ALIGN-1));
    }

    void *fb[3];
    for(int i=0;i<3;i++) fb[i]=(uint8_t*)vaddr+i*FB_SIZE;

    /* Skonfiguruj atrybut bufora: A8B8G8R8_SRGB, linear, 16:9 */
    OrbisVideoOutBufferAttribute attr;
    memset(&attr,0,sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr,
        ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB, /* 0x80002200 */
        1,  /* tmode: LINEAR */
        ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9,
        FB_W, FB_H, FB_W);
    ps4log("  attr: fmt=0x%x tmode=%d",attr.format,attr.tmode);

    /* Zarejestruj 3 bufory */
    r=sceVideoOutRegisterBuffers(vo,0,(void*const*)fb,3,&attr);
    ps4log("  registerBuffers: 0x%x",r);
    if(r<0){ ps4log("  FAIL"); return; }

    ps4log("  animacja 2s (gradient CPU -> VideoOut)...");
    for(int frame=0;frame<120;frame++){
        int idx=frame%3;
        uint32_t *px=(uint32_t*)fb[idx];
        float t=(float)frame/120.f;
        /* A8B8G8R8: wypelnij kolor zmieniajacy sie z czerwonego na niebieski */
        uint8_t red=(uint8_t)(t*255);
        uint8_t blue=(uint8_t)((1.f-t)*255);
        uint32_t col=0xFF000000u|((uint32_t)blue<<16)|(0x80u<<8)|(uint32_t)red;
        for(int i=0;i<FB_W*FB_H;i++) px[i]=col;
        /* Bialy prostokat w centrum = potwierdzenie ze rendering dziala */
        for(int y=FB_H/2-60;y<FB_H/2+60;y++)
            for(int x=FB_W/2-250;x<FB_W/2+250;x++)
                px[y*FB_W+x]=0xFFFFFFFF;
        sceVideoOutSubmitFlip(vo,idx,0,(int64_t)frame);
        sceKernelUsleep(16000);
    }
    ps4log("  VideoOut FB OK!");
}

/* --------------------------------------------------------------- pad */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    scePadInit();
    int h=scePadOpen(user,0,0,NULL);
    ps4log("  scePadOpen: 0x%x (OK jesli >0)",h);
    if(h<0) return;
    OrbisPadData d; unsigned last=0xffffffff;
    for(int i=0;i<120;i++){
        memset(&d,0,sizeof(d));
        scePadReadState(h,&d);
        if(d.buttons&&d.buttons!=last){
            last=d.buttons; ps4log("  buttons=0x%08x",(unsigned)d.buttons);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

/* --------------------------------------------------------------- audio */
static void probe_audio(void)
{
    ps4log("== audio (PERSONAL) ==");
    sceAudioOutInit();
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    int h=sceAudioOutOpen(user,5 /*PERSONAL*/,0,512,48000,
                          ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    ps4log("  open: 0x%x",h);
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
    sceAudioOutOutput(h,NULL); sceAudioOutClose(h);
    ps4log("  audio OK");
}

/* --------------------------------------------------------------- main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v9 " __DATE__ " " __TIME__);
    ps4log("PAID=0x3800000000000035");
    sceUserServiceInitialize(NULL);
    probe_piglet();
    probe_videoout_fb();
    probe_pad();
    probe_audio();
    ps4log("DONE");
    ps4_exit();
    return 0;
}
