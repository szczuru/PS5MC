// PS4MC Probe v2 – minimalna wersja diagnostyczna.
// BEZ Pigleta, BEZ jbc_jailbreak, BEZ sceKernelGetFsSandboxRandomWord.
// Cel: potwierdzić że aplikacja w ogóle startuje i pisze log.
// Po potwierdzeniu logu dodamy moduły z powrotem jeden po drugim.

#include <stdlib.h>  // przed <cmath>: libc++ potrzebuje ::abs
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
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// Własna implementacja abs żeby nie ciągnąć <cmath> z kolizjami
static inline double my_sin(double x) {
    // uproszczony sin przez szereg Taylora – tylko do testu audio
    double r=x, t=x; for(int i=1;i<10;i++){t*=-x*x/((2*i)*(2*i+1));r+=t;} return r;
}

static FILE* g_log = nullptr;

static void Log(const char* fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    std::fputs(buf, stdout); std::fputc('\n', stdout); std::fflush(stdout);
    if(g_log){ std::fputs(buf,g_log); std::fputc('\n',g_log); std::fflush(g_log); }
}

static void OpenLog()
{
    // Próbuje każdą ścieżkę po kolei
    mkdir("/data/ps4mc", 0777);
    mkdir("/data/ps4mc", 0755);
    const char* paths[] = {
        "/data/ps4mc/probe.log",
        "/mnt/usb0/ps4mc-probe.log",
        "/mnt/usb1/ps4mc-probe.log",
        "/tmp/ps4mc-probe.log",
        nullptr
    };
    for(int i=0; paths[i]; i++){
        g_log = std::fopen(paths[i], "w");
        if(g_log){ Log("LOG: %s", paths[i]); return; }
    }
    // Jeśli żadna ścieżka nie działa – dalej piszemy tylko na stdout
}

// ---------------------------------------------------------------- system
static void ProbeSystem()
{
    Log("== system ==");
    Log("uid=%d euid=%d pid=%d",
        (int)getuid(), (int)geteuid(), (int)getpid());

    struct stat st;
    const char* dirs[] = {
        "/data", "/app0", "/mnt/usb0", "/mnt/usb1",
        "/dev", "/system", "/dev/cd0", nullptr
    };
    for(int i=0; dirs[i]; i++){
        int e = stat(dirs[i], &st)==0 ? 0 : errno;
        Log("stat %-20s %s", dirs[i], e==0?"ok":strerror(e));
    }
}

// ---------------------------------------------------------------- pad
static void ProbePad()
{
    Log("== pad ==");
    int r = sceUserServiceInitialize(nullptr);
    Log("sceUserServiceInitialize: 0x%x", r);
    int32_t user = 0;
    r = sceUserServiceGetInitialUser(&user);
    Log("user: %d (ret 0x%x)", (int)user, r);
    r = scePadInit();
    Log("scePadInit: 0x%x", r);
    int h = scePadOpen(user, 0, 0, nullptr);
    Log("scePadOpen: %d", h);
    if(h < 0) return;

    // Czeka ~3 s na wciśnięcie przycisku (potwierdzenie że pad działa)
    Log("wcisnij dowolny przycisk na padzie (3 s)...");
    OrbisPadData d; uint32_t last = 0xffffffff;
    for(int i=0;i<180;i++){
        memset(&d,0,sizeof(d));
        if(scePadReadState(h,&d)==0 && d.buttons!=last){
            last=d.buttons;
            Log("  pad buttons=0x%08x connected=%u", d.buttons, (unsigned)d.connected);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

// ---------------------------------------------------------------- audio
static void ProbeAudio()
{
    Log("== audio ==");
    int r = sceAudioOutInit();
    Log("sceAudioOutInit: 0x%x", r);
    int32_t user=0; sceUserServiceGetInitialUser(&user);
    const uint32_t grain=512;
    int h = sceAudioOutOpen(user, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                            grain, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    Log("sceAudioOutOpen: 0x%x", h);
    if(h<0) return;
    std::vector<int16_t> buf(grain*2);
    double ph=0;
    // ~1 s tonu 440 Hz
    for(int i=0;i<48000/(int)grain;i++){
        for(uint32_t s=0;s<grain;s++){
            int16_t v=(int16_t)(my_sin(ph)*8000);
            buf[s*2]=buf[s*2+1]=v;
            ph+=2.0*3.14159265358979*440.0/48000.0;
        }
        sceAudioOutOutput(h, buf.data());
    }
    sceAudioOutOutput(h, nullptr);
    sceAudioOutClose(h);
    Log("audio OK (powinien byc ton ~1 s)");
}

int main()
{
    OpenLog();
    Log("PS4MC Probe v2 " __DATE__ " " __TIME__);
    Log("BUILD: minimal - bez Piglet, bez jbc");

    ProbeSystem();
    ProbePad();
    ProbeAudio();

    Log("== DONE == aplikacja dziala poprawnie");
    Log("Nastepny krok: wklej ten log - dodamy Piglet i jbc");

    if(g_log) std::fclose(g_log);
    sceKernelSleep(3);
    return 0;
}
