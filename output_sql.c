/*
 *  Copyright (C) 2020 Mateusz Jończyk
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "output_sql.h"
#include "main.h"

void display_sensor_reading_sql(FILE *stream, const int sensor_id,
                const struct device_single_measurement *measurement)
{
        fputs("INSERT INTO sensor_reading(metrics_state_id, sensor_id", stream);

                if (!DEVICE_IS_INCORRECT_TEMPERATURE(measurement->temperature)) {
                        fputs(", temperature", stream);
                }

                if (measurement->humidity != DEVICE_INCORRECT_HUMIDITY) {
                        fputs(", humidity", stream);
                }

                if (!DEVICE_IS_INCORRECT_TEMPERATURE(measurement->dew_point)) {
                        fputs(", dew_point", stream);
                }

        fprintf(stream, ") VALUES (@insert_id, %d", sensor_id);
                if (!DEVICE_IS_INCORRECT_TEMPERATURE(measurement->temperature)) {
                        fprintf(stream, ", %.2f", measurement->temperature);
                }

                if (measurement->humidity != DEVICE_INCORRECT_HUMIDITY) {
                        fprintf(stream, ", %d", measurement->humidity);
                }

                if (!DEVICE_IS_INCORRECT_TEMPERATURE(measurement->dew_point)) {
                        fprintf(stream, ", %.2f", measurement->dew_point);
                }
        fputs(");\n", stream);
}

void display_sensor_state_sql(FILE *stream, const struct device_sensor_state *state)
{
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time), false);

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
        fprintf(stream, "INSERT INTO metrics_state(time_utc) VALUES ('%s'); \n", current_time);
        fprintf(stream, "SET @insert_id = LAST_INSERT_ID();\n");
        if (state->station_sensor.any_data_present) {
                display_sensor_reading_sql(stream, 0,
                                &state->station_sensor.current);
        }
        for (int i=0; i<3; i++) {
                if (state->remote_sensors[i].any_data_present) {
                        display_sensor_reading_sql(stream, i+1,
                                &state->remote_sensors[i].current);
                }
        }

        fprintf(stream, "COMMIT;\n");

        fflush(stream);

}
