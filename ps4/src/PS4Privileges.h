#pragma once
// PS4MC – privilege escalation and sandbox escape via libXcape (libjbc).
//
// Użycie:
//   PS4Privileges::Escalate();   // raz na starcie, przed pierwszym dostępem do /dev/cd0
//   PS4Privileges::IsRoot();     // sprawdzenie
//   PS4Privileges::MountDisc();  // montuje /dev/cd0 pod sandbox jako "disc0"
//   PS4Privileges::Drop();       // opcjonalne: powrót do normalnych cred
//
// Wszystkie funkcje są thread-safe (wywołuj z jednego wątku na starcie).
// Po Escalate() aplikacja ma uid=0, pełne możliwości kernela i dostęp do
// całego drzewa plików – tak samo jak GoldHEN uruchamia /system homebrewy.

#ifdef __cplusplus
extern "C" {
#endif
#include "libjbc.h"
#ifdef __cplusplus
}
#endif

#include <string>

namespace PS4Privileges
{
  // Wynik operacji Escalate / Mount.
  enum class Result
  {
    OK = 0,
    AlreadyDone,      // Escalate() już była wywołana wcześniej
    JailbreakFailed,  // jbc_jailbreak zwrócił błąd (HEN nie jest aktywny?)
    MountFailed,      // jbc_mount_in_sandbox zwrócił błąd
    NotEscalated,     // MountDisc() bez wcześniejszego Escalate()
  };

  const char* ResultStr(Result r);

  // Eskaluje uprawnienia do root + escape z więzienia FreeBSD.
  // Wywołaj RAZ, zanim dotkniesz /dev/cd0, /system, USB itp.
  Result Escalate();

  // Montuje /dev/cd0 (napęd) wewnątrz sandboxa jako "disc0".
  // Potem dostępne jako /mnt/sandbox/disc0/ z poziomu aplikacji.
  // Wymaga wcześniejszego Escalate().
  Result MountDisc();
  void   UnmountDisc();

  // Montuje dowolną ścieżkę systemową wewnątrz sandboxa.
  Result MountSystem(const char* systemPath, const char* sandboxName);
  void   Unmount(const char* sandboxName);

  // Powrót do oryginalnych cred (np. przed wywołaniem kodu systemowego).
  void Drop();

  // Stan
  bool IsEscalated();
  bool IsRoot();
} // namespace PS4Privileges
