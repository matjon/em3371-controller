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

#include "output_mysql.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <mysql.h>

static MYSQL * mysql_ptr;

bool init_mysql_output(struct program_options *options)
{
        mysql_ptr = mysql_init(NULL);
        if (mysql_ptr == NULL) {
                fputs("Cannot create MySQL object\n", stderr);
                return false;
        }

        // mysql_optionsv() should be called before every mysql_real_connect()
        unsigned int timeout=5;
        mysql_optionsv(mysql_ptr, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&timeout);
        mysql_optionsv(mysql_ptr, MYSQL_OPT_READ_TIMEOUT, (void *)&timeout);
        mysql_optionsv(mysql_ptr, MYSQL_OPT_WRITE_TIMEOUT, (void *)&timeout);

        mysql_optionsv(mysql_ptr, MYSQL_READ_DEFAULT_FILE, NULL);

        if (mysql_real_connect(mysql_ptr, options->mysql_server,
                        options->mysql_user, options->mysql_password,
                        options->mysql_database,
                        0,
                        NULL, 0) == NULL) {
                fprintf(stderr, "Cannot connect to MySQL server: %s\n",
                                mysql_error(mysql_ptr));
                return false;
        }

        int ret = mysql_query(mysql_ptr, "insert into weather_data(time) values ('2020-12-14 23:00:55') ");
        const char *error_string = mysql_error(mysql_ptr);
        if (ret != 0 || strlen(error_string) != 0) {
                fprintf(stderr,
                        "mysql_query failed with return value %d and message \"%s\"\n",
                        ret, error_string);
        }

        mysql_store_result(mysql_ptr);

        mysql_close(mysql_ptr);

        return true;
}
