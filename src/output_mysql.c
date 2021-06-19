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
#include "output_sql.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <mysql.h>

static MYSQL * mysql_ptr = NULL;

static const char *mysql_server;
static const char *mysql_user;
static const char *mysql_password;
static const char *mysql_database;
static bool mysql_connected=false;

static bool mysql_try_connect()
{
        if (mysql_ptr != NULL) {
                mysql_close(mysql_ptr);
                mysql_ptr = NULL;
        }

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

        if (mysql_real_connect(mysql_ptr, mysql_server,
                        mysql_user, mysql_password,
                        mysql_database,
                        0,
                        NULL, 0) == NULL) {
                fprintf(stderr, "Cannot connect to MySQL server: %s\n",
                                mysql_error(mysql_ptr));

                mysql_connected=false;
        } else {
                mysql_connected=true;
        }

        return mysql_connected;
}

bool init_mysql_output(struct program_options *options)
{
        mysql_server = options->mysql_server;
        mysql_user = options->mysql_user;
        mysql_password = options->mysql_password;
        mysql_database = options->mysql_database;

        mysql_ptr = NULL;
        mysql_connected=false;

        mysql_try_connect();
        return true;
}

bool output_mysql_execute_statement(const char *statement)
{
        int ret = mysql_query(mysql_ptr, statement);
        const char *error_string = mysql_error(mysql_ptr);
        if (ret != 0 || strlen(error_string) != 0) {
                fprintf(stderr,
                        "mysql_query \"%s\" failed with return value %d and message \"%s\"\n",
                        statement, ret, error_string);

                return false;
        }

        MYSQL_RES * result = mysql_store_result(mysql_ptr);

        if (result != NULL) {
                 mysql_free_result(result);
        }
        return true;
}

bool store_sensor_state_mysql(const struct device_sensor_state *state)
{
        bool return_value = false;

        struct sql_statements_list statements;
        sql_statements_list_construct(&statements);
        get_sensor_state_sql(&statements, state);

        if (!mysql_connected && !mysql_try_connect()) {
                // TODO: store the data in a temporary buffer
                goto out;
        }

        if (! output_mysql_execute_statement("START TRANSACTION")) {
                // If we are not able to start a transaction, something is
                // not right. Try to reconnect.
                if (!mysql_try_connect() ||
                        ! output_mysql_execute_statement("START TRANSACTION")) {
                        goto out;
                }
        };

        for (unsigned i = 0; i < statements.count; i++) {
                if (!output_mysql_execute_statement(statements.statements[i])) {
                        mysql_rollback(mysql_ptr);

                        goto out;
                }
        }

        return_value = mysql_commit(mysql_ptr);

out:
        sql_statements_list_free(&statements);
        return return_value;
}


// TODO: shutdown
