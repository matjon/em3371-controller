Data logger for WiFi weather stations: Meteo SP73 and GreenBlue GB522
=====================================================================

A program to receive and decode network traffic from WiFi-enabled weather
stations: Meteo SP73 and GreenBlue GB522.

Program do przetwarzania danych wysyłanych przez stację pogody Meteo SP73.
Ta stacja pogody jest prawie identyczna z wyglądu do stacji pogody GreenBlue
GB522.
Stacje Meteo SP73 oraz GreenBlue GB522 wyglądają identycznie, poza logotypami
marki.

Program działa pod kontrolą Linuksa, w tym także DD-WRT. Powinien zadziałać bez
problemu na Raspberry Pi. Nie był testowany na OpenWRT, jednak nie powinno być
większych problemów z uruchomieniem go tam.

Przeniesienie na Windowsa (przy użyciu MinGW) wymagałoby pewnej ilości pracy,
jednak z użyciem biblioteki Gnulib mogłoby być stosunkowo proste.

Program jest oparty na pracy wykonanej przez Pana Łukasza Kalamłackiego, który
wykonał analizę wsteczną protokołu stacji:
http://kalamlacki.eu/sp73.php

## Opis konfiguracji stacji

Do zrobienia: opis konfiguracji stacji - zmiany docelowego adresu, do którego
są wysyłane dane.

Formaty wyjścia programu (JSON, CSV) mogą się zmienić w przyszłej wersji.

## Compilation for DD-WRT

It is necessary to use a toolchain that links code with the C library used in the DD-WRT build,
which frequently is uClibc. Cross compilers supplied with the distribution usually generate code for
GNU libc and so the program compiled with them may not work.

The compilers for DD-WRT can be downloaded from 
        https://dd-wrt.com/support/other-downloads/
path `toolchains/toolchains.tar.xz`. It is a large download, but only a part of it will be needed
--- it is not necessary to decompress the whole file.

It is not straightforward to determine which toolchain to use and also required command line
parameters of GCC. Some hints:

1. Login via SSH and check `/proc/cpuinfo` and top of `dmesg`. This will tell the architecture and
   the version of the compiler that was used to build the kernel.
2. Look at the DD-WRT source code and build scripts on https://github.com/mirror/dd-wrt/ .
   The source code is unfortunately messy and it is not easy to determine which build scripts are
   really used and which are just leftovers sitting idly in the repository.
   Proper build scripts seem to be in `src/router` (GCC command line parameters may be found in this
   file), toolchain used may be defined in `src/router/configs/buildscripts` (but files there may be
   out-of-date).

Some devices have no hardware floating point support. If so, it may be necessary to use
`-msoft-float` GCC command line parameter. Otherwise, the program may silently hang with possibly
the following message in dmesg:

    FPU emulator disabled, make sure your toolchainwas compiled with software floating point support (soft-float)

This problem occured with `toolchain-mipsel_gcc4.1.2` even with this command line parameter.
I therefore used `toolchain-mipsel_gcc-linaro_uClibc-0.9.32`, which works correctly.


## Usage on DD-WRT

- zalecane jest co najmniej 1MB wolnego miejsca na partycji /jffs,
  - partycja /jffs chyba nie jest domyślnie włączona, trzeba załączyć ją
    w panelu WEB GUI rutera i przed pierwszym użyciem sformatować,

  - jeśli nie ma takiej opcji, to można użyć mniejszych buildów DD-WRT,

- część danych można jednak trzymać w pamięci RAM, np. dzienniki debugowania
  (stderr programu),

- najlepiej jest użyć logrotate do rotacji i kompresji logów,
  - z załączoną opcją sharedscripts,

- plik CSV z jednego dnia nie skompresowany może zająć nawet 500kB, jeśli są obecne
  wszystkie 3 czujniki,

## TODO

- dew point calculation,
- implement log file saving and rotation (useful on devices with limited storage space),
    - using `logrotate` could be easier and better, however,
- runtime configuration, via a config file or command line parameters,
- write proper README.md,
- display time in local timezone on DD-WRT (may be difficult),
- reverse engineering:
    - devise a more accurate formula to calculate temperature from raw data,
    - describe how `historical1` and `historical2` in `device_single_sensor_data` behave,
    - decode other bytes in packets sent by the device,
    Some versions of the weather station are described to contain a DCF77 time
    receiver.
- use a Makefile instead of compile.sh
- Windows port
    - probably the best approach would be to use Gnulib:
            https://www.gnu.org/software/gnulib/
- maybe some refactoring,
- write text documentation of the protocol used by the weather station,
- a webpage that displays the last and historical measurements.
  It could be made using a single HTML file, with some JavaScript code that
  would download the results in JSON. The C program could provide a single text file
  that would contain only the last measurement in JSON.
- sending data to a site like wunderground.com,
- integration into other projects that handle weather station data (I
  could search them on GitHub),
