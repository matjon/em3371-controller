#!/bin/bash
# Compilation for DD-WRT - see:
#https://github.com/mirror/dd-wrt/blob/master/src/router/Makefile.brcm26
#https://github.com/mirror/dd-wrt/blob/master/src/router/configs/buildscripts/build_broadcom_K26.sh

# Using GCC 4.1.2 causes the program to hang on floating-point operations
# Therefore I'm using the other compiler, which seems to work correctly
#export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc4.1.2/bin:$PATH

make clean
export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc-linaro_uClibc-0.9.32/bin:$PATH

CC=mipsel-linux-uclibc-gcc                                                                              \
CFLAGS="-DBCMWPA2 -pipe -mips32 -mtune=mips32 -fno-caller-saves -mno-branch-likely -msoft-float"        \
make

# Additional possible GCC parameters: -minterlink-mips16 -mips16
