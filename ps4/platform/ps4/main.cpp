// PS4MC – entry point (szkielet M2)
// Na razie tylko placeholder – pełna implementacja po wynikach probe.

#include "PS4Privileges.h"

#include <cstdio>

// TODO: po udanym probe.log
// - init Piglet / EGL lub czysty VideoOut
// - pętla zdarzeń Kodi
// - AESink, Pad, VideoOut wrappery

int main(int argc, char* argv[])
{
  (void)argc;
  (void)argv;

  std::printf("[PS4MC] main() – szkielet\n");

  auto r = PS4Privileges::Escalate();
  std::printf("[PS4MC] Escalate: %s\n", PS4Privileges::ResultStr(r));

  // Tu później: CApplication, RenderSystem, itp.

  return 0;
}
