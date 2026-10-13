// PS4MC – PS4Privileges.cpp
// Implementacja eskalacji uprawnień i ucieczki z sandboxa via libXcape.

#include "PS4Privileges.h"

#include <cstdio>
#include <cstring>

namespace PS4Privileges
{

static jbc_jail_state s_state;
static bool           s_escalated = false;
static bool           s_discMounted = false;

const char* ResultStr(Result r)
{
  switch (r)
  {
    case Result::OK:              return "OK";
    case Result::AlreadyDone:    return "AlreadyDone";
    case Result::JailbreakFailed:return "JailbreakFailed";
    case Result::MountFailed:    return "MountFailed";
    case Result::NotEscalated:   return "NotEscalated";
  }
  return "Unknown";
}

Result Escalate()
{
  if (s_escalated)
    return Result::AlreadyDone;

  std::memset(&s_state, 0, sizeof(s_state));
  int r = jbc_jailbreak(&s_state);
  if (r != 0)
  {
    std::fprintf(stderr, "[PS4Priv] jbc_jailbreak failed: %d (HEN aktywny?)\n", r);
    return Result::JailbreakFailed;
  }
  s_escalated = true;
  std::fprintf(stdout, "[PS4Priv] Escalate OK – uid=%u\n",
               static_cast<unsigned>(s_state.root.uid));
  return Result::OK;
}

Result MountDisc()
{
  if (!s_escalated)
    return Result::NotEscalated;
  if (s_discMounted)
    return Result::AlreadyDone;

  // Montuje /dev/cd0 (napęd Blu-ray / DVD) w sandboxie pod nazwą "disc0".
  // Dostępne jako /mnt/sandbox/disc0/ wewnątrz procesu.
  int r = jbc_mount_in_sandbox("/dev/cd0", "disc0");
  if (r != 0)
  {
    std::fprintf(stderr, "[PS4Priv] MountDisc failed: %d\n", r);
    return Result::MountFailed;
  }
  s_discMounted = true;
  std::fprintf(stdout, "[PS4Priv] /dev/cd0 mounted -> /mnt/sandbox/disc0\n");
  return Result::OK;
}

void UnmountDisc()
{
  if (!s_discMounted)
    return;
  jbc_unmount_in_sandbox("disc0");
  s_discMounted = false;
}

Result MountSystem(const char* systemPath, const char* sandboxName)
{
  if (!s_escalated)
    return Result::NotEscalated;
  int r = jbc_mount_in_sandbox(systemPath, sandboxName);
  if (r != 0)
  {
    std::fprintf(stderr, "[PS4Priv] MountSystem(%s -> %s) failed: %d\n",
                 systemPath, sandboxName, r);
    return Result::MountFailed;
  }
  return Result::OK;
}

void Unmount(const char* sandboxName)
{
  jbc_unmount_in_sandbox(sandboxName);
}

void Drop()
{
  if (!s_escalated)
    return;
  jbc_unjailbreak(&s_state);
  s_escalated = false;
}

bool IsEscalated() { return s_escalated; }

bool IsRoot()
{
  if (!s_escalated)
    return false;
  jbc_cred cur{};
  jbc_get_cred(&cur);
  return cur.uid == 0;
}

} // namespace PS4Privileges
