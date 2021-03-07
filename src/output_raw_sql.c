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

#include "output_raw_sql.h"
#include "output_sql.h"
#include "main.h"

#include <stdio.h>

static FILE *sql_output_stream = NULL;
static bool sql_output_stream_close_on_exit = false;

bool init_sql_output(const char *output_path)
{
        return open_output_file(output_path, &sql_output_stream,
                        &sql_output_stream_close_on_exit, "SQL");
}

void shutdown_sql_output()
{
        close_output_file(&sql_output_stream, &sql_output_stream_close_on_exit);
}

void display_sensor_state_sql(const struct device_sensor_state *state)
{
        struct sql_statements_list statements;
        FILE *stream = sql_output_stream;

        fprintf(stream, "START TRANSACTION;\n");

        sql_statements_list_construct(&statements);
        get_sensor_state_sql(&statements, state);
        for (unsigned i = 0; i < statements.count; i++) {
                fprintf(stream, "%s;\n", statements.statements[i]);
        }
        sql_statements_list_free(&statements);

        fprintf(stream, "COMMIT;\n");
        fflush(stream);
}
