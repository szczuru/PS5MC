# PS4MC – Kodi na PS4: instrukcja krok po kroku

Wszystko robisz w przeglądarce, na GitHubie. Nie musisz niczego instalować na komputerze
(poza ewentualnie programem do FTP).

## Część 1 – fork i gałąź

1. Wejdź na https://github.com/VivaLaVent/PS5MC i kliknij **Fork** (prawy górny róg),
   potem **Create fork**.
2. W swoim forku kliknij listę gałęzi (napis `ps5mc-pier` lub `master` w lewym górnym rogu
   listy plików). Wpisz w polu wyszukiwania `ps5mc-piers`, wybierz ją.
3. Kliknij znowu listę gałęzi, wpisz `ps4-kodi-piers` i kliknij
   **Create branch ps4-kodi-piers from ps5mc-piers**.
4. Ustaw ją jako domyślną: **Settings → Branches → Default branch → ikona strzałek →
   `ps4-kodi-piers` → Update**. (Bez tego przycisk „Run workflow” nie pokaże się w Actions.)
5. Zakładka **Actions** → kliknij **I understand my workflows, go ahead and enable them**.

## Część 2 – wgranie tego pakietu

1. Rozpakuj ZIP na komputerze.
2. W forku, na gałęzi `ps4-kodi-piers`: **Add file → Upload files**.
3. Przeciągnij do okna **zawartość** rozpakowanego folderu: foldery `.github` i `ps4`
   oraz plik `START-TUTAJ.md`. (Foldery zaczynające się od kropki bywają ukryte –
   włącz „pokaż ukryte pliki” w eksploratorze.)
4. Na dole: wybierz **Commit directly to the ps4-kodi-piers branch** → **Commit changes**.

Commit w folderze `ps4/` uruchamia budowanie automatycznie.

## Część 3 – pobranie programu testowego

1. Zakładka **Actions** → wpis **PS4 build** (żółta kropka = trwa, zielony haczyk = gotowe,
   zwykle kilka minut).
2. Kliknij w ukończone uruchomienie → na dole **Artifacts** → `ps4mc-probe-pkg` (ZIP z plikiem
   `IV0000-PS4MC0001_00-PS4MCPROBE000000.pkg`).
3. Jeśli coś się wysypało (czerwony krzyżyk): wejdź w uruchomienie, kliknij nieudany krok
   i wklej mi ostatnie ~30 linii logu. Poprawię.

## Część 4 – instalacja na PS4

Potrzebujesz PS4 po jailbreaku z GoldHEN (lub innym HEN z instalacją pakietów).

1. Skopiuj plik `.pkg` na konsolę przez FTP (GoldHEN uruchamia serwer FTP na porcie 2121)
   do katalogu `/data/pkg/` – albo na pendrive exFAT/FAT32.
2. Na PS4: **Ustawienia → Debug Settings → Game → Package Installer** (nazwa i położenie
   zależą od wersji HEN) i zainstaluj pakiet.
3. Uruchom aplikację **PS4MC Probe** z ekranu głównego. Pierwsze ~minuty:
   - ok. 8 s możesz naciskać przyciski pada (są logowane),
   - usłyszysz 2 s dźwięku,
   - zobaczysz kolorową animację na ekranie (to test grafiki Piglet).
   Włóż płytę z filmem do napędu przed startem, jeśli chcesz sprawdzić odczyt płyty.
4. Po zakończeniu pobierz log przez FTP: `/data/ps4mc/probe.log`
   (awaryjnie `/mnt/usb0/ps4mc-probe.log`). **Wklej mi całą zawartość** – na jego podstawie
   dobiorę ścieżkę grafiki i dekodera.

Jeśli aplikacja zawiesi się lub wróci do ekranu głównego bez logu, napisz, w którym
momencie (obraz? dźwięk?). To też jest informacja.

## Co dalej

Patrz `ps4/docs/PLAN.md`.

## Licencje i źródła

- Kodi: GPL-2.0-or-later (ten sam fork).
- Toolchain: OpenOrbis PS4 Toolchain (pobierany podczas budowania, nie jest częścią repo).
- Pliki `right.sprx`, `libc.prx`, `libSceFios2.prx` do paczki są pobierane z repozytorium
  OpenOrbis w czasie budowania.
- Repo nie zawiera kodu ani bibliotek Sony.
