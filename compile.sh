SOURCES="main.c emax_em3371.c psychrometrics.c output_json.c output_csv.c output_sql.c"
OUTPUT_BASE=emax_em3371_decoder
COMMON_PARAMETERS="-O2 -std=c99 -Wall -Wextra"
#COMMON_PARAMETERS="-O0 -g -std=c99 -Wall -Wextra"

gcc $COMMON_PARAMETERS $SOURCES -o ${OUTPUT_BASE} -lm

gcc $COMMON_PARAMETERS psychrometrics.c psychrometrics_test.c -o psychrometrics_test -lm


if [ "$1" == 'dd-wrt' ]; then
        # Compilation for DD-WRT - see:
        #https://github.com/mirror/dd-wrt/blob/master/src/router/Makefile.brcm26
        #https://github.com/mirror/dd-wrt/blob/master/src/router/configs/buildscripts/build_broadcom_K26.sh

        # Using GCC 4.1.2 causes the program to hang on floating-point operations
        # Therefore I'm using the other compiler, which seems to work correctly
        #export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc4.1.2/bin:$PATH
        export PATH=/media/1T-noraid/dd-wrt/toolchain-mipsel_gcc-linaro_uClibc-0.9.32/bin:$PATH
        mipsel-linux-uclibc-gcc                                         \
                -DBCMWPA2 -pipe -mips32 -mtune=mips32                   \
                -fno-caller-saves  -mno-branch-likely -msoft-float      \
                $COMMON_PARAMETERS                                      \
                $SOURCES -o ${OUTPUT_BASE}_mipsel -lm

        mipsel-linux-uclibc-gcc                                         \
                -DBCMWPA2 -pipe -mips32 -mtune=mips32                   \
                -fno-caller-saves  -mno-branch-likely -msoft-float      \
                $COMMON_PARAMETERS                                      \
                psychrometrics.c psychrometrics_test.c                  \
                -o psychrometrics_test_mipsel -lm

        # Additional possible GCC parameters: -minterlink-mips16 -mips16
fi
