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

#define SQL_INSERT_CONDITIONAL(NAME, SOURCE, FORMAT, CONDITION)         \
        const char *NAME##_field = "";                                  \
        char NAME##_str[10] = "";                                       \
        if ((CONDITION)) {                                              \
                NAME##_field = ", " #NAME;                              \
                snprintf(NAME##_str, sizeof(NAME##_str),                \
                                ", " FORMAT, SOURCE);                   \
        }

void display_sensor_state_debug(FILE *stream, const int sensor_id,
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

        fprintf(stream,
                "INSERT INTO sensor_reading_debug(metrics_state_id, sensor_id"
                "%s%s%s%s%s"
                ") VALUES (@insert_id, %d"
                "%s%s%s%s%s"
                ");\n",
                temperature_min_field, temperature_max_field,
                humidity_min_field, humidity_max_field,
                payload_0x31_field,

                sensor_id,

                temperature_min_str, temperature_max_str,
                humidity_min_str, humidity_max_str,
                payload_0x31_str);
}

void display_sensor_reading_sql(FILE *stream, const int sensor_id,
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

        fprintf(stream, "INSERT INTO sensor_reading(metrics_state_id, sensor_id"
                "%s%s%s%s, battery_low"
                ") VALUES (@insert_id, %d"
                "%s%s%s%s%s);\n",
                temperature_field, humidity_field,
                dew_point_field, atmospheric_pressure_field,

                sensor_id,
                temperature_str, humidity_str,
                dew_point_str, atmospheric_pressure_str,
                battery_low_str);
}

void display_sensor_state_sql(FILE *stream, const struct device_sensor_state *state)
{
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time), false);

	char device_time_str[30];
	tm_to_string(&(state->device_time), device_time_str, sizeof(device_time_str));

// TODO: Grafana really requires time stored in the database to be in UTC timezone:
//
//      https://community.grafana.com/t/preset-time-frames-broken-shows-no-data-points-date-is-5-hours-ahead/10683/9
//      Our intention have never been to support anything else than UTC on Grafana and UTC in
//      database/storing timestamps in actual UTC time in database. Unfortunately this have worked
//      for you unintentionally before. Hopefully we can find a way to manage multiple timezones in
//      dashboard/datasource in the future.
//
//      When the data disappears when zooming in, this is likely to be a timezone issue.

        fprintf(stream, "START TRANSACTION;\n");
        fprintf(stream, "INSERT INTO metrics_state(time_utc, device_time) "
                        "VALUES ('%s', '%s'); \n",
                        current_time, device_time_str);

        fprintf(stream, "SET @insert_id = LAST_INSERT_ID();\n");
        if (state->station_sensor.any_data_present) {
                display_sensor_reading_sql(stream, 0,
                                &state->station_sensor,
                                state->atmospheric_pressure);
                display_sensor_state_debug(stream, 0, &state->station_sensor,
                                state->payload_byte_0x31);
        }
        for (int i=0; i<3; i++) {
                if (state->remote_sensors[i].any_data_present) {
                        display_sensor_reading_sql(stream, i+1,
                                &state->remote_sensors[i],
                                DEVICE_INCORRECT_PRESSURE);

                        display_sensor_state_debug(stream, i+1, &state->remote_sensors[i], 0);
                }
        }

        fprintf(stream, "COMMIT;\n");

        fflush(stream);

}
