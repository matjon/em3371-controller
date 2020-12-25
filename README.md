Program to capture data from some LivingSense-compatible weather stations
=========================================================================

A program to receive and decode network traffic from some WiFi-enabled weather
stations - the many rebranded versions of EMAX EM3371:
- Meteo SP73,
- GreenBlue GB522,
- Meteo Logic EM3371,
- DIGOO DG-EX001.

This program has been tested on a Meteo SP73 weather station, which was bought
in late 2020. It is based on reverse-engineering work performed by Mr Łukasz
Kalamłacki: http://kalamlacki.eu/sp73.php

Notice: this is a third party program. It is neither endorsed nor supported by
EMAX or the above weather station brands. All trademarks are used only for
the purpose of showing compatibility or potential compatibility.

Usage reports are very welcome - through a GitHub bug report or a mail to
"mat.jonczyk" in the domain "o2.pl".

**Warning**: this program is under active development. Interface stability is
not guaranteed.

Supported operating systems
---------------------------

This program was written in a way that enabled it to be used on constrained
resource devices, such as home routers.

This program runs on GNU/Linux and DD-WRT. It was not tested on OpenWRT, but
modifying it to work there should be simple. It should run without problems on
a Raspberry Pi with GNU/Linux installed.

Windows is currently not supported, but I may work on it in the future
(possibly using the Gnulib library).

Configuring the weather station
-------------------------------

If the weather station has not already been connected to a WiFi network:

1. Press the "WIFI / UP" button on the back of the device for three seconds.
   The "AP" text will appear on the weather station display (in the place
   where the day of week is usually shown).
2. Connect to the WiFi network called "LivingSmart".
3. Open in a web browser the address http://11.11.11.254/. Be patient - the
   WebGUI is *very* slow and can take up to several minutes to load.
4. Login using "admin" / "admin" credentials. The menu is all in Chinese, but
   the identifiers in the HTML code are in English. An online translator may
   also help.
5. Select "STA设置" in the menu on the left hand side and wait until the page
   will fully load (the version number will appear on the top then).
6. Press "搜索" in the top row on the right. Select the appropriate WiFi access
   point (typing in the name in the text field may not work).
7. Configure other settings. It may be a good idea to disable DHCP
   ("自动获得IP地址" = Disable) and type in an incorrect gateway ("网关地址").
   This way the device will be able to access only the local network.
   Of course, access could also be blocked on a firewall.
8. Submit the form, then on the next screen choose "重启" ("restart, reboot").
   It seems that it is the WiFi module that is restarted, not the whole weather
   station.
9. Again press the "WIFI/UP" button for three seconds - until the "AP" text will
   be replaced with the day of week.

After the device has been connected to the WiFi network:

1. Open in a web browser the IP address of the device.
2. Login using "admin"/"admin" credentials.
3. In the menu on the left hand side, select "其他设置". After a while a new
   window will load, wait till is loaded fully and some values will show in the
   form. In the bottom there is an option to specify server ("服务器地址",
   default value "SMARTSERVER.EMAXTIME.CN") and port number ("端口", default
   value 17000) to which the device will send sensor data.
4. Submit the form, then select "重启" ("restart, reboot").

When the weather station WiFi module is running in the Access Point mode, the
WebGUI is very sluggish, but in STA mode (when connected to a WiFi network as a
client device) it responds much faster.

Probably the device does not send measurement packets in Access Point mode.

It seems likely that after a power cycle the reporting server is reset to the
default value and has to be set again. In the future, I may write a script to
reconfigure the device automatically if this occurs.

Take care not to modify the serial port settings in the configuration interface
(they are above the knobs to specify the reporting servers) as this may make
the device inoperable. Also, the WebGUI has an option that allows to reset the
settings. IMHO, Doing this may be risky, however.


## General remarks on weather station use

A good guide on weather station sensor placement can be found at
        https://www.wunderground.com/pws/installation-guide

The wireless sensor really cannot be put anywhere where it receives sunlight -
it is black and heats up much in this case. It reported temperature up to 28°C
in winter.

## Compilation for DD-WRT

It is necessary to use a toolchain that links code with the C library used in the DD-WRT build,
which frequently is uClibc. Cross compilers supplied with Linux distributions
usually generate code for GNU libc and so the program compiled with them may
not work.

Compilers for DD-WRT can be obtained from
        https://dd-wrt.com/support/other-downloads/
path `toolchains/toolchains.tar.xz`. It is a large download, but only a part of it will be needed
--- it is not necessary to decompress the whole file.

It is not straightforward to determine which toolchain to use and also required command line
parameters of GCC. Some hints:

1. Login via SSH and check `/proc/cpuinfo` and top of `dmesg`. This will tell the architecture and
   version of the compiler that was used to build the kernel.
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

- implement log file saving and rotation (useful especially on devices with
  limited storage space),
    - using `logrotate` could be easier and better, however,
- handle '--help' command line parameter,
- write proper README.md,
- reverse engineering:
    - byte 0x31 of payload from function 0x01,
- use a Makefile instead of compile.sh
- Windows port
    - probably the best approach would be to use Gnulib:
            https://www.gnu.org/software/gnulib/
- maybe some refactoring,
- a webpage that displays the last and historical measurements.
  It could be made using a single HTML file, with some JavaScript code that
  would download the results in JSON. The C program could provide a single text file
  that would contain only the last measurement in JSON.
  - but writing a WeeWX plugin would mostly cover that use case,
- sending data to a site like wunderground.com,
- integration into other projects that handle weather station data (I
  could search them on GitHub),
  - I did some research and writing a WeeWX plugin seems sensible,
  - or perhaps modifying wview, but it seems not to be maintained any longer,

- find other weather stations that should be compatible with this software,
  - searching for "Livingsense" (the mobile app name) may be helpful,
- dew point calculation: check which parameters to choose when the dry bulb
  temperature is above 0°C, but the dew point / frost point is below 0°C,

## Links

        pon, 2 lis 2020, 18:07:07 CET
        - weather station Meteo Logic EM3371 looks similar, so it
        may also be compatible:
        https://www.youtube.com/watch?v=G7D54pm1FjE

        DIGOO DG-EX001 should also be the same model rebranded,

        EDIT: pon, 2 lis 2020, 18:59:16 CET
        http://www.emaxtime.com/product/13.html
                - product info on the manufacturer's website, where the station
                  is called "EM3371 Wifi weather [sic]"
