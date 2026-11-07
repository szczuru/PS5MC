# PS4MC – Kodi na PS4: instrukcja + probe v12

## Co masz w tym folderze

- `probe/` – program testowy (v12) – sprawdza VideoOut, audio, pad, Piglet
- `scripts/` – budowanie PKG przez OpenOrbis
- `toolchain/ps4-kodi.cmake` – plik CMake do późniejszego pełnego buildu Kodi
- `src/PS4Privileges.*` – eskalacja uprawnień (libXcape / jbc)
- `docs/PLAN.md` – plan etapów portu
- `title/sce_sys/icon0.png` – ikona

## Problem: czarny ekran (dźwięk działa)

Probe v12 jest specjalnie przygotowany pod ten przypadek:

1. **VideoOut jest testowany NAJPIERW** (Piglet na końcu – wcześniej mógł zabierać display).
2. Najpierw 720p linear A8B8G8R8, potem 1080p, potem inne formaty.
3. Dłuższe czasy (biały ~5 s, gradient ~8 s, kolory solidne).
4. `FLIP_VSYNC` + czekanie na `sceVideoOutIsFlipPending`.
5. Pełne logowanie każdego kodu powrotu.

### Co powinieneś zobaczyć (jeśli grafika działa)

1. ~5 sekund **pełnego białego ekranu**
2. ~8 sekund gradientu czerwony → niebieski z białym prostokątem na środku
3. Po kolei: czerwony / zielony / niebieski / żółty (po ~2 s)
4. Potem dźwięk 440 Hz (~1 s)
5. Krótki test pada (możesz klikać)
6. Na końcu próba Piglet (animacja koloru)

Jeśli nadal czarny ekran – **to normalne na tym etapie**. Najważniejszy jest log.

## Budowanie przez GitHub Actions

1. Wgraj zmiany (folder `ps4/` + `.github/workflows/ps4-build.yml`) na gałąź `ps4-kodi-piers`.
2. Zakładka **Actions** → **PS4 build**.
3. Po sukcesie pobierz artifact **ps4mc-probe-pkg**.
4. Zainstaluj `.pkg` na PS4 (GoldHEN → Package Installer lub FTP do `/data/pkg/`).

## Po uruchomieniu probe

1. Poczekaj aż aplikacja sama wyjdzie (albo naciśnij PS).
2. Pobierz log przez FTP:
   - `/data/kodi/probe.log` (główny)
   - awaryjnie `/mnt/usb0/kodi-probe.log`
3. **Wklej całą zawartość logu** – na jego podstawie dobiorę dalszą ścieżkę (format, tiling, pamięć GPU, Piglet).

## Co dalej (po udanym logu)

Patrz `docs/PLAN.md`:

- **M2** – szkielet platformy PS4 w Kodi (`xbmc/platform/ps4/`)
- **M3** – zależności (FFmpeg, Python…)
- **M4** – pełny build Kodi + odblokowanie joba `kodi` w Actions

## Licencje

- Kodi: GPL-2.0-or-later
- Toolchain: OpenOrbis (pobierany w czasie budowania)
- Repo nie zawiera kodu ani bibliotek Sony
