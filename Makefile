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

all: emax_em3371_decoder psychrometrics_test

MAIN_DEPENDENCIES = src/main.o src/emax_em3371.o src/psychrometrics.o 	\
		    src/output_json.o src/output_csv.o src/output_sql.o	\
		    src/output_raw_sql.o

MYSQL_DEPENDENCIES = src/output_mysql.o src/output_mysql_buffer.o

PSYCH_TEST_DEPS = src/psychrometrics.o src/psychrometrics_test.o

LDLIBS := -lm
CFLAGS := $(CFLAGS) -std=c99 -Wall -Wextra

ifeq ($(MARIADB), 1)
	DEPENDENCIES := $(MAIN_DEPENDENCIES) $(MYSQL_DEPENDENCIES)
	CFLAGS := $(CFLAGS) -DHAVE_MYSQL $(shell mariadb_config --include)
	LDLIBS := $(LDLIBS) $(shell mariadb_config --libs)
else
	DEPENDENCIES = $(MAIN_DEPENDENCIES)
endif

emax_em3371_decoder: $(DEPENDENCIES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(LOADLIBES)

psychrometrics_test: $(PSYCH_TEST_DEPS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(LOADLIBES)



ALL_DEPS := $(DEPENDENCIES) $(PSYCH_TEST_DEPS)
DEP_FILES := $(ALL_DEPS:.o=.d)
-include $(DEP_FILES)

%.o: %.c
	$(CC) -c $(CFLAGS) -MM -MF $(patsubst %.o,%.d,$@) -o $@ $<
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	-rm emax_em3371_decoder psychrometrics_test src/*.o src/*.d
