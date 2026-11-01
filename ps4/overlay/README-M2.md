# PS4MC – M2 Platform Overlay

Te pliki to **szkielet platformy PS4** pod Kodi (etap M2).

## Struktura

```
ps4/overlay/
├── cmake/platform/ps4/PS4.cmake
└── xbmc/platform/ps4/
    ├── main.cpp              ← punkt wejścia
    ├── PlatformPS4.*         ← CPlatform
    ├── VideoOutInfo.*        ← działający VideoOut (z probe v12)
    ├── CPUInfoPS4.*
    ├── MemUtils.cpp
    ├── PS4InterfaceForCLog.*
    ├── CMakeLists.txt
    ├── audio/
    │   ├── AESinkPS4.*       ← audio PERSONAL (działa)
    │   └── CMakeLists.txt
    ├── input/
    │   ├── PS4PadInput.*     ← pad (szkielet)
    │   └── CMakeLists.txt
    └── video/
        └── CMakeLists.txt
```

## Jak użyć

1. Skopiuj zawartość `overlay/` do roota repo Kodi (nadpisze/utworzy `xbmc/platform/ps4/` i `cmake/platform/ps4/`).
2. Upewnij się, że `ps4/src/PS4Privileges.*` i `ps4/libXcape` są dostępne.
3. Toolchain: `ps4/toolchain/ps4-kodi.cmake`
4. Pełny build Kodi pojawi się w kolejnym etapie (M3/M4) – na razie to szkielet, który kompiluje się razem z resztą gdy odblokujemy job `kodi`.

## Stan na podstawie probe v12

| Element | Status |
|---------|--------|
| VideoOut 720p/1080p linear A8B8G8R8 | ✅ działa |
| DirectMemory | ✅ |
| Audio PERSONAL | ✅ |
| Pad | ✅ (handle) |
| Piglet / GLES | ❌ eglGetDisplay FAIL – na razie idziemy VideoOut |

## Następne kroki

- Podpiąć `CVideoOutInfo` pod windowing / RenderSystem (na razie software path)
- Rozwinąć mapping pada
- Odblokować job `kodi` w Actions gdy zależności (M3) będą gotowe
