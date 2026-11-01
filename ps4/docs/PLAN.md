# PS4MC – Plan portu Kodi na PS4

## Etapy

### ✅ M0 – Szkielet i toolchain (gotowe)
- Toolchain: OpenOrbis PS4 Toolchain (LLVM 18, FreeBSD 12)
- Plik CMake do krzyżowej kompilacji (`ps4/toolchain/ps4-kodi.cmake`)
- Skrypty budowania + tworzenia PKG
- GitHub Actions (`.github/workflows/ps4-build.yml`)
- Biblioteka uprawnień libXcape (`jbc_jailbreak`, `jbc_mount_in_sandbox`)

### ✅ M1 – Program testowy (probe) – **SUKCES**
- Plik: `ps4/probe/main.c` (v12)
- **VideoOut 720p linear A8B8G8R8_SRGB działa** (kolory, gradienty, kwadraty widoczne)
- Audio PERSONAL działa
- Pad otwiera się
- Piglet: `eglGetDisplay(0)` = FAIL → idziemy ścieżką VideoOut (bez GLES na start)

### ✅ M2 – Szkielet platformy PS4 w Kodi (gotowy)
Pliki w `ps4/overlay/` → kopiować do `xbmc/platform/ps4/` i `cmake/platform/ps4/`:
- `main.cpp`, `PlatformPS4.*`
- `VideoOutInfo.*` – działający VideoOut z probe
- `AESinkPS4.*` – audio PERSONAL
- `PS4PadInput.*` – pad
- CMake + reszta szkieletu

### 🟡 M3 – Zależności (w toku)
Skrypty w `ps4/depends/scripts/`:
- zlib, bzip2, libpng, freetype
- openssl, curl
- **FFmpeg** (software decode H.264/HEVC itd.)

Job `depends` w GitHub Actions jest aktywny.
Cel: `ps4/depends/prefix/{lib,include}` z gotowymi `.a`.

### 🔲 M4 – Pełny build Kodi
- `ps4/scripts/build-kodi.sh` (do napisania po udanym M3)
- Aktywacja joba `kodi` w Actions
- Kodi uruchamia się, widać interfejs

### 🔲 M5 – Płyty DVD/Blu-ray
### 🔲 M6 – Sprzętowy dekoder (opcjonalnie)

---

## Mapa PS5 → PS4 (kluczowe pliki)

| Plik PS5 (`xbmc/platform/ps5/`)  | Odpowiednik PS4                              | Status |
|-----------------------------------|----------------------------------------------|--------|
| `main.cpp`                        | `overlay/.../main.cpp`                       | szkielet |
| `SandboxPS5.cpp`                  | `ps4/src/PS4Privileges.cpp`                  | ✅ |
| `VideoOutInfo` / video            | `VideoOutInfo.*` (VideoOut path)             | ✅ działa |
| `audio/AESinkPS5.cpp`             | `AESinkPS4.cpp`                              | ✅ |
| `input/PS5PadInput.cpp`           | `PS4PadInput.cpp`                            | szkielet |
| toolchain                         | `ps4/toolchain/ps4-kodi.cmake`               | ✅ |

---

## Aktualny stan grafiki
- **Główna ścieżka:** czysty VideoOut + CPU framebuffer (720p/1080p linear A8B8G8R8)
- Piglet/GLES: na razie nie działa (`eglGetDisplay` FAIL)
- Hardware video decode: później (M6)
