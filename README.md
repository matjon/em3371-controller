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

## TODO

- CSV output,
- implement log file saving and rotation (for limited space),
- runtime configuration, via a config file or command line parameters,
- write proper README.md,
- display time in local timezone on DD-WRT (if it's easy),
- reverse engineering:
    - devise a more accurate formula to calculate temperature from raw data,
    - describe how `historical1` and `historical2` in `device_single_sensor_data` behaves,
    - decode other bytes in the packets,
- use a Makefile instead of compile.sh
