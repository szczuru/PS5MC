/* PS4MC Probe v4
   - poprawne wyjscie przez sceSystemServiceLoadExec("exit", NULL)
   - eskalacja uprawnien przez libXcape (jbc_jailbreak)
   - po eskalacji: uid=0, dostep do /mnt/disc, /system
   - test padu i audio (1s ton)                                     */

#include <stdlib.h>
#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>
#include <orbis/SystemService.h>
#include "libjbc.h"   /* libXcape */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------------------------------------------------------------- log */
static int g_log_fd = -1;

static void log_write(const char *msg)
{
    ssize_t n = (ssize_t)strlen(msg);
    if (g_log_fd >= 0) { write(g_log_fd, msg, (size_t)n); write(g_log_fd, "\n", 1); }
    write(1, msg, (size_t)n); write(1, "\n", 1);
}
static void ps4log(const char *fmt, ...)
{
    char buf[512]; va_list ap;
    va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    log_write(buf);
}
static void open_log(void)
{
    mkdir("/data/ps4mc", 0777);
    const char *p[] = {"/data/ps4mc/probe.log",
                       "/mnt/usb0/ps4mc-probe.log",
                       "/tmp/ps4mc-probe.log", NULL};
    for (int i = 0; p[i]; i++) {
        g_log_fd = open(p[i], O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (g_log_fd >= 0) { ps4log("LOG: %s", p[i]); return; }
    }
}

/* ---------------------------------------------------------------- exit */
static void ps4_exit(void)
{
    if (g_log_fd >= 0) { close(g_log_fd); g_log_fd = -1; }
    sceSystemServiceLoadExec("exit", NULL);
    for (;;) sceKernelSleep(1); /* nigdy tu nie dojdziemy */
}

/* ---------------------------------------------------------------- system */
static void probe_stat(const char *path)
{
    struct stat st;
    int e = stat(path, &st);
    ps4log("stat %-22s %s", path, e==0 ? "OK" : strerror(errno));
}

static void probe_system(void)
{
    ps4log("== system ==");
    ps4log("uid=%d euid=%d pid=%d", (int)getuid(), (int)geteuid(), (int)getpid());
    probe_stat("/data");
    probe_stat("/app0");
    probe_stat("/dev");
    probe_stat("/mnt/disc");
    probe_stat("/mnt/usb0");
    probe_stat("/system");
    probe_stat("/system/common/lib");
}

/* ---------------------------------------------------------------- jbc */
static void probe_escalate(void)
{
    ps4log("== jbc / eskalacja uprawnien ==");
    static jbc_jail_state state;
    int r = jbc_jailbreak(&state);
    ps4log("jbc_jailbreak: %d (0=ok)", r);
    ps4log("po jbc: uid=%d euid=%d", (int)getuid(), (int)geteuid());

    if (r == 0) {
        /* Sprawdz co teraz widac */
        probe_stat("/mnt/disc");
        probe_stat("/mnt/usb0");
        probe_stat("/system");
        probe_stat("/system/common/lib");

        /* Sprobuj zamontowac /mnt/disc w sandboxie (wymaga plyt) */
        int mr = jbc_mount_in_sandbox("/mnt/disc", "disc");
        ps4log("jbc_mount_in_sandbox(/mnt/disc -> disc): %d (0=ok)", mr);
        if (mr == 0) {
            probe_stat("/mnt/sandbox/disc");
            /* Odczyt sektora 16 plyt ISO (naped musi miec plyte) */
            int fd = open("/mnt/sandbox/disc", O_RDONLY);
            if (fd >= 0) {
                unsigned char sec[2048];
                off_t o = lseek(fd, (off_t)16 * 2048, SEEK_SET);
                ssize_t n = read(fd, sec, sizeof(sec));
                if (o >= 0 && n == 2048)
                    ps4log("sektor 16 plyt: type=%u id='%.5s'",
                         sec[0], (const char*)&sec[1]);
                else
                    ps4log("odczyt sektora: %zd (%s)", n, strerror(errno));
                close(fd);
                jbc_unmount_in_sandbox("disc");
            } else {
                ps4log("open /mnt/sandbox/disc0: %s", strerror(errno));
            }
        }

        /* Sprawdz moduly dekodera (czy plik istnieje) */
        const char *mods[] = {
            "/system/common/lib/libSceVideodec.sprx",
            "/system/common/lib/libSceVideodec2.sprx",
            "/system/common/lib/libScePigletv2VSH.sprx",
            "/system/common/lib/libScePrecompiledShaders.sprx",
            "/system/common/lib/libSceShaccVSH.sprx",
            NULL
        };
        ps4log("-- moduly systemowe --");
        for (int i = 0; mods[i]; i++) {
            struct stat st;
            int e = stat(mods[i], &st);
            ps4log("  %-44s %s", mods[i], e==0?"OK":"brak");
        }
    }
}

/* ---------------------------------------------------------------- pad */
static void probe_pad(void)
{
    ps4log("== pad ==");
    int32_t user = 0;
    int r = sceUserServiceGetInitialUser(&user);
    ps4log("user: %d (ret 0x%x)", (int)user, r);

    r = scePadInit();
    ps4log("scePadInit: 0x%x", r);

    int h = scePadOpen(user, 0, 0, NULL);
    ps4log("scePadOpen: 0x%x (%s)", h, h>=0?"OK":"blad");
    if (h < 0) return;

    ps4log("nacisnic przycisk (2 s)...");
    OrbisPadData d; unsigned last = 0xffffffff;
    for (int i = 0; i < 120; i++) {
        memset(&d, 0, sizeof(d));
        scePadReadState(h, &d);
        if (d.buttons && d.buttons != last) {
            last = d.buttons;
            ps4log("  buttons=0x%08x connected=%u",
                 (unsigned)d.buttons, (unsigned)d.connected);
        }
        sceKernelUsleep(16000);
    }
    scePadClose(h);
}

/* ---------------------------------------------------------------- audio */
static void probe_audio(void)
{
    ps4log("== audio ==");
    int r = sceAudioOutInit();
    ps4log("sceAudioOutInit: 0x%x", r);
    int32_t user = 0; sceUserServiceGetInitialUser(&user);
    const unsigned grain = 512;
    int h = sceAudioOutOpen(user, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                            grain, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    ps4log("sceAudioOutOpen: 0x%x", h);
    if (h < 0) return;

    short buf[512*2]; double ph = 0.0;
    for (int f = 0; f < 48000/(int)grain; f++) { /* ~1 s tonu 440 Hz */
        for (unsigned s = 0; s < grain; s++) {
            short v = (short)(sin(ph) * 8000.0);
            buf[s*2] = buf[s*2+1] = v;
            ph += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
        }
        sceAudioOutOutput(h, buf);
    }
    sceAudioOutOutput(h, NULL);
    sceAudioOutClose(h);
    ps4log("audio OK");
}

/* ---------------------------------------------------------------- main */
int main(void)
{
    open_log();
    ps4log("PS4MC Probe v4 " __DATE__ " " __TIME__);

    sceUserServiceInitialize(NULL);

    probe_system();
    probe_escalate();
    probe_pad();
    probe_audio();

    ps4log("DONE");
    ps4_exit();   /* nigdy nie wracamy przez return */
    return 0;
}
