/* PS4MC Probe v3 – czysty C, absolutne minimum.
   Cel: potwierdzic ze aplikacja dociera do main() i zapisuje log.
   Zero C++, zero libXcape, zero Piglet. */

#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/AudioOut.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_log_fd = -1;

static void log_write(const char *msg)
{
    ssize_t len = (ssize_t)strlen(msg);
    if (g_log_fd >= 0) { write(g_log_fd, msg, (size_t)len); write(g_log_fd, "\n", 1); }
    write(1, msg, (size_t)len); write(1, "\n", 1); /* stdout */
}

static void logf(const char *fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    log_write(buf);
}

static void open_log(void)
{
    mkdir("/data/ps4mc", 0777);
    const char *paths[] = {
        "/data/ps4mc/probe.log",
        "/mnt/usb0/ps4mc-probe.log",
        "/mnt/usb1/ps4mc-probe.log",
        "/tmp/ps4mc-probe.log",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        g_log_fd = open(paths[i], O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (g_log_fd >= 0) { logf("LOG: %s", paths[i]); return; }
    }
}

int main(void)
{
    open_log();
    logf("PS4MC Probe v3 C " __DATE__ " " __TIME__);
    logf("uid=%d euid=%d pid=%d", (int)getuid(), (int)geteuid(), (int)getpid());

    /* Sprawdz kilka katalogow */
    struct stat st;
    const char *dirs[] = { "/data", "/app0", "/dev", "/dev/cd0",
                           "/mnt/usb0", "/system", NULL };
    for (int i = 0; dirs[i]; i++) {
        int e = stat(dirs[i], &st);
        logf("stat %-16s %s", dirs[i], e==0 ? "OK" : strerror(errno));
    }

    /* Pad */
    int r = sceUserServiceInitialize(NULL);
    logf("sceUserServiceInitialize: 0x%x", r);
    int32_t user = 0;
    sceUserServiceGetInitialUser(&user);
    logf("user: %d", (int)user);
    r = scePadInit();
    logf("scePadInit: 0x%x", r);
    int ph = scePadOpen(user, 0, 0, NULL);
    logf("scePadOpen: %d", ph);

    if (ph >= 0) {
        logf("nacisnic przycisk na padzie (2 s)...");
        OrbisPadData d;
        for (int i = 0; i < 120; i++) {
            memset(&d, 0, sizeof(d));
            scePadReadState(ph, &d);
            if (d.buttons)
                logf("  buttons=0x%08x", d.buttons);
            sceKernelUsleep(16000);
        }
        scePadClose(ph);
    }

    logf("DONE – aplikacja dziala!");
    if (g_log_fd >= 0) close(g_log_fd);
    sceKernelSleep(3);
    return 0;
}
