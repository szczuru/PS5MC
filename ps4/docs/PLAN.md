# PS4MC – Plan portu Kodi na PS4

## Etapy

### ✅ M0 – Szkielet i toolchain (gotowe)
- Toolchain: OpenOrbis PS4 Toolchain (LLVM 18, FreeBSD 12, musl)
- Plik CMake do krzyżowej kompilacji (`ps4/toolchain/ps4-kodi.cmake`)
- Skrypty budowania + tworzenia PKG
- GitHub Actions (`.github/workflows/ps4-build.yml`)
- Biblioteka uprawnień libXcape (`jbc_jailbreak`, `jbc_mount_in_sandbox`)

### ✅ M1 – Program testowy (probe) (gotowe)
- Plik: `ps4/probe/main.cpp`
- Sprawdza na konsoli: uid/root, dostęp do `/dev/cd0`, moduły dekoderów,
  GLES Piglet (wersja, rozszerzenia, kompilacja GLSL, benchmark YUV 1080p),
  pad DS4, audio wyjście
- **Wynik M1 = log z konsoli**, który rozstrzyga wybór ścieżki grafiki i dekodera.

### 🔲 M2 – Szkielet platformy PS4 w Kodi
Pliki do stworzenia w `xbmc/platform/ps4/` i `cmake/platform/ps4/`:
- `main.cpp` – punkt wejścia, init Piglet/EGL, pętla zdarzeń
- `RenderSystemGLES_PS4.cpp` – wrapper GLES (prawdopodobnie bez zmian vs PS5)
- `VideoOutPS4.cpp` – VideoOut przez `sceVideoOut` (istnieje na PS4)
- `AESinkPS4.cpp` – audio przez `sceAudioOut`
- `PS4PadInput.cpp` – DS4 przez `scePad` + `sceImeDialog`
- `CMakeLists.txt` + `cmake/platform/ps4/PS4.cmake`

Zależność: wyniki z M1 (czy GLSL działa? który decoder?).

### 🔲 M3 – Zależności (FFmpeg, Python, biblioteki)
Cross-build pod toolchain PS4 musl:
- FFmpeg z dekodowaniem programowym H.264/HEVC (fallback)
- Jeśli M1 potwierdzi Videodec2: FFmpeg hwaccel przez `libSceVideodec2.sprx`
- libpython 3.11 (stabilna, sprawdzona z libjbc)
- libsmb2, libnfs, curl, zlib, libjpeg, libpng, FreeType

### 🔲 M4 – Pełny build Kodi
- `ps4/scripts/build-kodi.sh`
- Aktywacja zadania `kodi` w GitHub Actions
- Kodi uruchamia się, widać interfejs, można grać muzyką/filmami z sieci

### 🔲 M5 – Płyty DVD/Blu-ray
- Zależy od M1 (czy `/dev/cd0` dostępne po `jbc_jailbreak`)
- DVD-Video: czytamy ISO 9660, przekazujemy ścieżkę do FFmpeg
- Blu-ray: wymaga libdvdread + libaacs (klucze AACS z zewnątrz)

### 🔲 M6 – Sprzętowy dekoder (opcjonalnie po M4)
Tylko jeśli M1 pokaże, że `libSceVideodec2.sprx` ładuje się i symbole są dostępne.

---

## Mapa PS5 → PS4 (kluczowe pliki)

| Plik PS5 (`xbmc/platform/ps5/`)  | Odpowiednik PS4                              | Uwagi                          |
|-----------------------------------|----------------------------------------------|--------------------------------|
| `main.cpp`                        | `ps4/main.cpp`                               | Inna warstwa uprawnień (jbc)   |
| `SandboxPS5.cpp`                  | `ps4/PS4Privileges.cpp` ✅                   | Zrobione via libXcape          |
| `video/VideoOutPS5.cpp`           | `ps4/VideoOutPS4.cpp`                        | `sceVideoOut` istnieje na PS4  |
| `video/DVDVideoCodecPS5.cpp`      | `ps4/DVDVideoCodecPS4.cpp`                   | Inne API (`Videodec` nie `Videodec2` jako główne) |
| `audio/AESinkPS5.cpp`             | `ps4/AESinkPS4.cpp`                          | `sceAudioOut` identyczne API   |
| `input/PS5PadInput.cpp`           | `ps4/PS4PadInput.cpp`                        | `scePad` identyczne API; DS4 zamiast DualSense |
| `elf/PS5ElfLoader.cpp`            | nieużywane (loadujemy `.sprx` przez kernel)  | OpenOrbis ma `sceKernelLoadStartModule` |
| `VideoOutInfo.cpp`                | `ps4/VideoOutInfo.cpp`                       | Minor zmiany (brak HDR/VRR)    |
| `ps5/toolchain/ps5-kodi.cmake`    | `ps4/toolchain/ps4-kodi.cmake` ✅            | Zrobione                       |

---

## Ryzyka i decyzje oczekujące na M1

| Ryzyko | Co sprawdza M1 | Jeśli OK | Jeśli FAIL |
|--------|---------------|----------|------------|
| GLSL runtime compile | `GLSL runtime compile: vertex=OK` | Kodi GLES renderer bez zmian | Precompile shaderów offline (trudne) |
| Piglet działa | `EGL 1.x` w logu | Grafika HW | Port Mesa (bardzo trudne) |
| Videodec2 symbole | `libSceVideodec2.sprx resolved` | HW decode M6 | FFmpeg SW decode tylko |
| Root po jbc | `uid=0` w logu | Płyty, /system | Ograniczona funkcjonalność |
| Dostęp do /dev/cd0 | `open /dev/cd0: ok` | DVD/BD | Tylko sieć/USB |

---

## Znane ograniczenia (celowo pominięte)

- HDR, VRR, Dolby Vision – brak API na PS4 dla HDR (HDMI 2.0 bez HDR10 w firmware)
- DualSense haptyki – DS4 nie ma (używamy `scePad` standardowo)
- Audio passthrough HDMI bitstream – może działać przez `sceAudioOut` w trybie `DIGITAL_OUT`, do zbadania po M4
