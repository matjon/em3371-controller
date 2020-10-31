SOURCES="main.c meteo_sp73.c"
OUTPUT_BASE=sp73_decoder
COMMON_PARAMETERS="-std=gnu99 -Wall -Wextra"

gcc $COMMON_PARAMETERS $SOURCES -o ${OUTPUT_BASE}

gcc $COMMON_PARAMETERS psychrometrics.c psychrometrics_test.c -o psychrometrics_test -lm


# Compilation for DD-WRT - see:
#https://github.com/mirror/dd-wrt/blob/master/src/router/Makefile.brcm26
#https://github.com/mirror/dd-wrt/blob/master/src/router/configs/buildscripts/build_broadcom_K26.sh

# Using GCC 4.1.2 causes the program to hang on floating-point operations
# Therefore I'm using the other compiler, which seems to work correctly
#export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc4.1.2/bin:$PATH
export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc-linaro_uClibc-0.9.32/bin:$PATH
mipsel-linux-uclibc-gcc                                         \
        -DBCMWPA2 -O2 -pipe -mips32 -mtune=mips32               \
        -fno-caller-saves  -mno-branch-likely -msoft-float      \
        $COMMON_PARAMETERS                                      \
        $SOURCES -o ${OUTPUT_BASE}_mipsel

mipsel-linux-uclibc-gcc                                         \
        -DBCMWPA2 -O2 -pipe -mips32 -mtune=mips32               \
        -fno-caller-saves  -mno-branch-likely -msoft-float      \
        $COMMON_PARAMETERS                                      \
        psychrometrics.c psychrometrics_test.c                  \
        -o psychrometrics_test_mipsel -lm

# Additional possible GCC parameters: -minterlink-mips16 -mips16

