# Kodi na PS4 (PS4MC) – szkielet i program testowy

Ten pakiet dodajesz do swojego forka `VivaLaVent/PS5MC`, na nowej gałęzi `ps4-kodi-piers`.
Zawiera:

- `.github/workflows/ps4-build.yml` – budowanie na GitHub Actions,
- `ps4/` – toolchain, skrypty, **program testowy (probe)**, dokumentacja.

Program testowy NIE jest jeszcze Kodi. Sprawdza na Twojej konsoli to, od czego zależy cały
port (grafika, dekodery, uprawnienia, płyta, pad, audio). Szczegóły: `ps4/README.md`
(krok po kroku) i `ps4/docs/PLAN.md` (plan i ryzyka).
