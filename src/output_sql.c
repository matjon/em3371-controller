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


#include "output_sql.h"
#include "main.h"

#include <stdlib.h>
#include <string.h>


#define SQL_INSERT_CONDITIONAL(NAME, SOURCE, FORMAT, CONDITION)         \
        const char *NAME##_field = "";                                  \
        char NAME##_str[10] = "";                                       \
        if ((CONDITION)) {                                              \
                NAME##_field = ", " #NAME;                              \
                snprintf(NAME##_str, sizeof(NAME##_str),                \
                                ", " FORMAT, SOURCE);                   \
        }

static size_t get_single_sensor_state_debug_sql(char *output, size_t output_space,
                const int sensor_id,
                const struct device_single_sensor_data *sensor_data,
                const unsigned char payload_byte_0x31)
{
        SQL_INSERT_CONDITIONAL(temperature_min,
                sensor_data->historical_min.temperature,
                "%.2f",
                !DEVICE_IS_INCORRECT_TEMPERATURE(sensor_data->historical_min.temperature)
                )

        SQL_INSERT_CONDITIONAL(temperature_max,
                sensor_data->historical_max.temperature,
                "%.2f",
                !DEVICE_IS_INCORRECT_TEMPERATURE(sensor_data->historical_max.temperature)
                )

        SQL_INSERT_CONDITIONAL(humidity_min,
                (int) sensor_data->historical_min.humidity,
                "%d",
                sensor_data->historical_min.humidity != DEVICE_INCORRECT_HUMIDITY
                )

        SQL_INSERT_CONDITIONAL(humidity_max,
                (int) sensor_data->historical_max.humidity,
                "%d",
                sensor_data->historical_max.humidity != DEVICE_INCORRECT_HUMIDITY
                )

        SQL_INSERT_CONDITIONAL(payload_0x31,
                (int) payload_byte_0x31,
                "%d",
                sensor_id == 0
                )

        return snprintf(output, output_space,
                "INSERT INTO sensor_reading_debug(metrics_state_id, sensor_id"
                "%s%s%s%s%s"
                ") VALUES (@insert_id, %d"
                "%s%s%s%s%s"
                ")",
                temperature_min_field, temperature_max_field,
                humidity_min_field, humidity_max_field,
                payload_0x31_field,

                sensor_id,

                temperature_min_str, temperature_max_str,
                humidity_min_str, humidity_max_str,
                payload_0x31_str);
}

static size_t get_single_sensor_state_sql(char *output, size_t output_space,
                const int sensor_id,
                const struct device_single_sensor_data *sensor_data,
                uint16_t atmospheric_pressure)
{
        SQL_INSERT_CONDITIONAL(temperature,
                sensor_data->current.temperature,
                "%.2f",
                !DEVICE_IS_INCORRECT_TEMPERATURE(sensor_data->current.temperature)
                )

        SQL_INSERT_CONDITIONAL(humidity,
                sensor_data->current.humidity,
                "%d",
                sensor_data->current.humidity != DEVICE_INCORRECT_HUMIDITY
                )

        SQL_INSERT_CONDITIONAL(dew_point,
                sensor_data->current.dew_point,
                "%.2f",
                !DEVICE_IS_INCORRECT_TEMPERATURE(sensor_data->current.dew_point)
                )

        SQL_INSERT_CONDITIONAL(atmospheric_pressure,
                atmospheric_pressure,
                "%d",
                atmospheric_pressure != DEVICE_INCORRECT_PRESSURE
                )

        const char *battery_low_str;
        if (sensor_data->battery_low) {
                battery_low_str = ", b\'1\'";
        } else {
                battery_low_str = ", b\'0\'";
        }


        return snprintf(output, output_space,
                "INSERT INTO sensor_reading(metrics_state_id, sensor_id"
                "%s%s%s%s, battery_low"
                ") VALUES (@insert_id, %d"
                "%s%s%s%s%s)",
                temperature_field, humidity_field,
                dew_point_field, atmospheric_pressure_field,

                sensor_id,
                temperature_str, humidity_str,
                dew_point_str, atmospheric_pressure_str,
                battery_low_str);
}

bool sql_statements_list_construct(struct sql_statements_list *statements)
{
        statements->count=0;
        statements->statements = malloc(SQL_STATEMENTS_MAX_COUNT
                        * sizeof(statements->statements[0]));
        if (statements->statements == NULL) {
                return false;
        }
        statements->max_count = SQL_STATEMENTS_MAX_COUNT;

        statements->memory = malloc(SQL_STATEMENTS_MEMORY_SIZE);
        if (statements->memory == NULL) {
                free(statements->statements);
                statements->statements = NULL;

                return false;
        }

        statements->memory_left = SQL_STATEMENTS_MEMORY_SIZE;
        statements->next_statement_place = statements->memory;
        return true;
}

void sql_statements_list_free(struct sql_statements_list *statements)
{
        free(statements->statements);
        statements->statements = NULL;

        free(statements->memory);
        statements->memory = NULL;

        statements->max_count = 0;
        statements->memory_left = 0;
        statements->next_statement_place = NULL;
}

bool sql_statements_list_arrange_next(struct sql_statements_list *statements)
{
        if (statements->count >= statements->max_count) {
                return false;
        }

        if (statements->memory_left == 0) {
                return false;
        }

        size_t length = strlen(statements->next_statement_place);
        if (length+1 > statements->memory_left) {
                fputs("Error: memory corruption detetcted in "
                        "sql_statements_list_arrange_next.\n", stderr);
                abort();
        }
        statements->memory_left -= (length+1);

        if (length == 0) {
                return true;
        }

        statements->statements[statements->count++] = statements->next_statement_place;
        statements->next_statement_place += (length+1);

        if (statements->memory_left == 0) {
                statements->next_statement_place = NULL;
        }

        return true;
}


void get_sensor_state_sql(
                struct sql_statements_list *statements,
                const struct device_sensor_state *state)
{
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time), false);

	char device_time_str[30];
	time_to_string(state->device_time, device_time_str, sizeof(device_time_str));

// TODO: Grafana really requires time stored in the database to be in UTC timezone:
//
//      https://community.grafana.com/t/preset-time-frames-broken-shows-no-data-points-date-is-5-hours-ahead/10683/9
//      Our intention have never been to support anything else than UTC on Grafana and UTC in
//      database/storing timestamps in actual UTC time in database. Unfortunately this have worked
//      for you unintentionally before. Hopefully we can find a way to manage multiple timezones in
//      dashboard/datasource in the future.
//
//      When the data disappears when zooming in, this is likely to be a timezone issue.

        snprintf(statements->next_statement_place,
                statements->memory_left,
                "INSERT INTO metrics_state(time_utc, device_time) "
                "VALUES ('%s', '%s')",
                current_time, device_time_str);
        sql_statements_list_arrange_next(statements);

        snprintf(statements->next_statement_place,
                statements->memory_left,
                "SET @insert_id = LAST_INSERT_ID()");
        sql_statements_list_arrange_next(statements);

        if (state->station_sensor.any_data_present) {
                get_single_sensor_state_sql(
                                statements->next_statement_place,
                                statements->memory_left,
                                0, &state->station_sensor,
                                state->atmospheric_pressure);
                sql_statements_list_arrange_next(statements);

                get_single_sensor_state_debug_sql(
                                statements->next_statement_place,
                                statements->memory_left,
                                0, &state->station_sensor,
                                state->payload_byte_0x31);
                sql_statements_list_arrange_next(statements);
        }
        for (int i=0; i<3; i++) {
                if (state->remote_sensors[i].any_data_present) {
                        get_single_sensor_state_sql(
                                statements->next_statement_place,
                                statements->memory_left,
                                i+1,
                                &state->remote_sensors[i],
                                DEVICE_INCORRECT_PRESSURE);

                        sql_statements_list_arrange_next(statements);
                        get_single_sensor_state_debug_sql(
                                statements->next_statement_place,
                                statements->memory_left,
                                i+1, &state->remote_sensors[i], 0);
                        sql_statements_list_arrange_next(statements);
                }
        }
}
