/*
 *  Copyright (C) 2020-2021 Mateusz Jończyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "output_mysql_buffer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static long mysql_buffer_max_entries = 0;
static struct device_sensor_state* mysql_buffer = NULL;

long push_position;
long pop_position;
long entries_in_buffer;

static void assert_internal_state()
{
        assert(push_position >=0);
        assert(pop_position >=0);

        if (mysql_buffer_max_entries != 0) {
                assert(push_position < mysql_buffer_max_entries);
                assert(pop_position < mysql_buffer_max_entries);
        }

        if (entries_in_buffer == mysql_buffer_max_entries) {
                assert(push_position == pop_position);
        } else {
                long suggested_entries = push_position - pop_position;
                if (suggested_entries < 0) {
                        suggested_entries += mysql_buffer_max_entries;
                }
                assert(suggested_entries == entries_in_buffer);
        }
}

bool init_mysql_buffer(size_t buffer_size)
{
        if (buffer_size == 0) {
                return true;
        }

        mysql_buffer = malloc(buffer_size);
        if (mysql_buffer == NULL) {
                fprintf(stderr, "Cannot allocate MySQL buffer of %zd bytes.\n",
                                buffer_size);
                return false;
        }

        mysql_buffer_max_entries = buffer_size / sizeof(struct device_sensor_state);

        fprintf(stderr, "MySQL buffer of size %zd bytes allocated, "
                "can store %lu entries.\n",
                buffer_size, mysql_buffer_max_entries);

        push_position = 0;
        pop_position = 0;
        entries_in_buffer = 0;

        assert_internal_state();

        return true;
}

void shutdown_mysql_buffer()
{
        free(mysql_buffer);

        mysql_buffer = NULL;
        mysql_buffer_max_entries = 0;

        push_position = 0;
        pop_position = 0;
        entries_in_buffer = 0;

        assert_internal_state();
}

bool store_in_mysql_buffer(const struct device_sensor_state *state)
{
        if (mysql_buffer_max_entries == 0) {
                return false;
        }

        if (entries_in_buffer >= mysql_buffer_max_entries) {
                discard_from_mysql_buffer();

                //        XXXXXX*XXXXXXX
                //        XXXXXXX*XXXXXX

                // In this case, after the function will have finished,
                // pop_position == push_position
        }

        memcpy(&mysql_buffer[push_position], state, sizeof(*state));

        //        00XXXXXX000000
        //        00XXXXXXX00000
        push_position = (push_position + 1) % mysql_buffer_max_entries;
        entries_in_buffer++;

        assert_internal_state();

        return true;
}

bool peek_from_mysql_buffer(struct device_sensor_state *state) {
        if (mysql_buffer_max_entries == 0) {
                return false;
        }
        if (entries_in_buffer == 0) {
                return false;
        }

        memcpy(state, &mysql_buffer[pop_position], sizeof(*state));
        return true;
}

bool discard_from_mysql_buffer()
{
        if (mysql_buffer_max_entries == 0) {
                return false;
        }
        if (entries_in_buffer == 0) {
                return false;
        }
        pop_position = (pop_position+1) % mysql_buffer_max_entries;
        //        00000*XXXXX000
        //        000000XXXXX000
        entries_in_buffer--;

        assert_internal_state();

        return true;
}

bool pop_from_mysql_buffer(struct device_sensor_state *state)
{
        return peek_from_mysql_buffer(state) && discard_from_mysql_buffer();
}

long get_mysql_buffer_count()
{
        return entries_in_buffer;
}
