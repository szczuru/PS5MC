/* PS4MC Probe v10
   GLOWNA ZMIANA: sceVideoOutSetFlipRate(vo, 0) – bez tego submitFlip
   jest ignorowany i ekran pozostaje czarny.
   Test: najpierw pelna biel (widoczna w kazdym formacie),
   potem gradient czerwony->niebieski z bialym prostokatem.        */

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
    ps4log("== Piglet (informacja) ==");
    const char *sw=sceKernelGetFsSandboxRandomWord();
    char p[128];
    snprintf(p,sizeof(p),"/%s/common/lib/libScePigletv2VSH.sprx",sw?sw:"");
    int ms=0; uint32_t h=sceKernelLoadStartModule(p,0,NULL,0,NULL,&ms);
    ps4log("  load: 0x%08x  (0x8d=OK)",h);
    if((int32_t)h<0) return;

    OrbisPglConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    cfg.processOrder=1;
    cfg.flags=ORBIS_PGL_FLAGS_USE_COMPOSITE_EXT|ORBIS_PGL_FLAGS_USE_FLEXIBLE_MEMORY;
    cfg.systemSharedMemorySize  =128ull*1024*1024;
    cfg.videoSharedMemorySize   =256ull*1024*1024;
    cfg.maxMappedFlexibleMemory = 64ull*1024*1024;
    cfg.drawCommandBufferSize   =  1  *1024*1024;
    cfg.lcueResourceBufferSize  =  1  *1024*1024;
    cfg.dbgPosCmd_0x40=1920; cfg.dbgPosCmd_0x44=1080; cfg.unk_0x5C=2;
    scePigletSetConfigurationVSH(&cfg);

    typedef EGLDisplay (*pfnGD)(int);
    pfnGD gd=(pfnGD)(void*)eglGetDisplay;
    EGLDisplay dpy=gd(0);
    ps4log("  eglGetDisplay(0): %s",(dpy!=EGL_NO_DISPLAY)?"OK – PIGLET DZIALA!":"FAIL (oczekiwane bez KE)");
    if(dpy!=EGL_NO_DISPLAY){
        EGLint mj=0,mn=0; eglInitialize(dpy,&mj,&mn);
        ps4log("  EGL %d.%d  GL: %s",mj,mn,glGetString(GL_VERSION));
        /* GLSL test */
        eglBindAPI(EGL_OPENGL_ES_API);
        const EGLint att[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
            EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_NONE};
        EGLConfig ec; EGLint n=0; eglChooseConfig(dpy,att,&ec,1,&n);
        OrbisPglWindow win={0,1920,1080,0};
        const EGLint wa[]={EGL_RENDER_BUFFER,EGL_BACK_BUFFER,EGL_NONE};
        EGLSurface surf=eglCreateWindowSurface(dpy,ec,&win,wa);
        const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        EGLContext ctx=eglCreateContext(dpy,ec,EGL_NO_CONTEXT,ca);
        if(eglMakeCurrent(dpy,surf,surf,ctx)){
            const char *vs="attribute vec4 p;void main(){gl_Position=p;}";
            const char *fs="precision mediump float;void main(){gl_FragColor=vec4(0.0,0.5,1.0,1.0);}";
            GLuint v=glCreateShader(GL_VERTEX_SHADER),f=glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(v,1,&vs,NULL); glCompileShader(v);
            glShaderSource(f,1,&fs,NULL); glCompileShader(f);
            GLint ov=0,of=0;
            glGetShaderiv(v,GL_COMPILE_STATUS,&ov);
            glGetShaderiv(f,GL_COMPILE_STATUS,&of);
            ps4log("  GLSL: VS=%s FS=%s",ov?"OK":"FAIL",of?"OK":"FAIL");
            for(int i=0;i<180;i++){
                glClearColor((float)i/180.f,0.3f,0.8f,1.f);
                glClear(GL_COLOR_BUFFER_BIT);
                eglSwapBuffers(dpy,surf);
            }
            ps4log("  animacja Piglet OK!");
        }
    }
}

/* --------------------------------------------------------------- VideoOut FB */
#define FB_W     1920
#define FB_H     1080
#define FB_SIZE  ((size_t)FB_W * FB_H * 4)
#define FB_ALIGN (2*1024*1024)

static void probe_videoout_fb(void)
{
    ps4log("== VideoOut framebuffer ==");

    int32_t user=0; sceUserServiceGetInitialUser(&user);
    int vo=sceVideoOutOpen(user,0,0,NULL);
    ps4log("  sceVideoOutOpen(user=%d): %d (0x%x)",user,vo,vo);
    if(vo<0){ ps4log("  FAIL"); return; }

    /* KLUCZOWE: ustaw flip rate przed rejestracją buforów */
    int r=sceVideoOutSetFlipRate(vo,0);
    ps4log("  sceVideoOutSetFlipRate(0): 0x%x",r);

    /* Alokuj pamiec Direct (GPU-visible) */
    const size_t aligned=((FB_SIZE*3+FB_ALIGN-1)/FB_ALIGN)*FB_ALIGN;
    off_t phys=0;
    r=(int)sceKernelAllocateDirectMemory(0,(off_t)0x200000000LL,
        aligned,FB_ALIGN,3,&phys);
    ps4log("  AllocateDirectMemory: 0x%x phys=0x%lx",r,(unsigned long)phys);

    void *vaddr=NULL;
    if(r==0){
        r=(int)sceKernelMapDirectMemory(&vaddr,aligned,0x33,0,phys,FB_ALIGN);
        ps4log("  MapDirectMemory: 0x%x vaddr=%p",r,vaddr);
    }
    if(r!=0||!vaddr){
        /* Fallback: malloc z recznym alignmentem */
        void *raw=malloc(FB_SIZE*3+FB_ALIGN);
        if(!raw){ ps4log("  malloc FAIL"); return; }
        vaddr=(void*)(((uintptr_t)raw+FB_ALIGN-1)&~(uintptr_t)(FB_ALIGN-1));
        ps4log("  malloc fallback vaddr=%p",vaddr);
    }

    void *fb[3];
    for(int i=0;i<3;i++) fb[i]=(uint8_t*)vaddr+i*FB_SIZE;

    OrbisVideoOutBufferAttribute attr;
    memset(&attr,0,sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr,
        ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB,
        1, /* LINEAR tiling – CPU fill */
        ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9,
        FB_W, FB_H, FB_W);

    r=sceVideoOutRegisterBuffers(vo,0,(void*const*)fb,3,&attr);
    ps4log("  registerBuffers: 0x%x",r);
    if(r<0){ ps4log("  FAIL"); return; }

    /* Faza 1: pelna biel (2 s) – widoczna niezaleznie od formatu */
    ps4log("  faza 1: pełna biel (2 s)...");
    for(int frame=0;frame<120;frame++){
        int idx=frame%3;
        uint32_t *px=(uint32_t*)fb[idx];
        for(int i=0;i<FB_W*FB_H;i++) px[i]=0xFFFFFFFF;
        sceVideoOutSubmitFlip(vo,idx,0,(int64_t)frame);
        sceKernelUsleep(16000);
    }

    /* Faza 2: gradient czerwony->niebieski + bialy prostokat (2 s) */
    ps4log("  faza 2: gradient + prostokąt (2 s)...");
    for(int frame=0;frame<120;frame++){
        int idx=frame%3;
        uint32_t *px=(uint32_t*)fb[idx];
        float t=(float)frame/120.f;
        /* A8B8G8R8: A=FF, B=blue, G=40, R=red */
        uint8_t red=(uint8_t)((1.f-t)*255);
        uint8_t blue=(uint8_t)(t*255);
        uint32_t col=0xFF000000u|((uint32_t)blue<<16)|(0x40u<<8)|(uint32_t)red;
        for(int i=0;i<FB_W*FB_H;i++) px[i]=col;
        /* Bialy prostokat srodkowy */
        for(int y=FB_H/2-80;y<FB_H/2+80;y++)
            for(int x=FB_W/2-300;x<FB_W/2+300;x++)
                px[y*FB_W+x]=0xFFFFFFFF;
        sceVideoOutSubmitFlip(vo,idx,0,(int64_t)(120+frame));
        sceKernelUsleep(16000);
    }
    ps4log("  VideoOut FB DONE");
    ps4log("  Czy widziales: 2s biel -> 2s gradient z prostokatem?");
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
    ps4log("== audio (PERSONAL=2) ==");
    sceAudioOutInit();
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    /* port 2 = PERSONAL, potwierdzony w v7 */
    int h=sceAudioOutOpen(user,2,0,512,48000,
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
    ps4log("PS4MC Probe v10 " __DATE__ " " __TIME__);
    sceUserServiceInitialize(NULL);
    probe_piglet();
    probe_videoout_fb();
    probe_pad();
    probe_audio();
    ps4log("DONE");
    ps4_exit();
    return 0;
}
