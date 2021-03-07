/*
 *  Copyright (C) 2020 Mateusz Jończyk
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


#include <stdio.h>
#include "emax_em3371.h"

struct sql_statements_list {
        unsigned int count;
        char **statements;

// private
        unsigned int max_count;
        char *memory;
        char *next_statement_place;
        size_t memory_left;
};
#define SQL_STATEMENTS_MAX_COUNT 20
#define SQL_STATEMENTS_MEMORY_SIZE 8*1024

bool sql_statements_list_construct(struct sql_statements_list *statements);
void sql_statements_list_free(struct sql_statements_list *statements);

/*
void sql_statements_list_get(struct sql_statements_list *statements,
                const struct device_sensor_state *state);
*/
void display_sensor_state_sql(FILE *stream, const struct device_sensor_state *state);
