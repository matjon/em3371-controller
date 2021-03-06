# Copyright (C) 2020-2021 Mateusz Jończyk
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


SOURCES="src/main.c src/emax_em3371.c src/psychrometrics.c src/output_json.c src/output_csv.c src/output_sql.c"
OUTPUT_BASE=emax_em3371_decoder
COMMON_PARAMETERS="-O2 -std=c99 -Wall -Wextra"
#COMMON_PARAMETERS="-O0 -g -std=c99 -Wall -Wextra"

if [ "$1" == 'mariadb' ]; then
        SOURCES="$SOURCES src/output_mysql.c"
        MARIADB_PARAMS="-DHAVE_MYSQL $(mariadb_config  --include --libs)"
fi
gcc $COMMON_PARAMETERS $SOURCES -o ${OUTPUT_BASE} -lm $MARIADB_PARAMS

gcc $COMMON_PARAMETERS src/psychrometrics.c src/psychrometrics_test.c -o psychrometrics_test -lm


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
                src/psychrometrics.c src/psychrometrics_test.c          \
                -o psychrometrics_test_mipsel -lm

        # Additional possible GCC parameters: -minterlink-mips16 -mips16
fi
