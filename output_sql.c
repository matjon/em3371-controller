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

        fprintf(stream, "INSERT INTO weather_data("
                                "time,atmospheric_pressure,"
                                "station_temp,station_humidity,station_dew_point,"
                                "sensor1_temp,sensor1_humidity,sensor1_dew_point) "
                "VALUES ('%s+00:00',%d, "
                        "%.2f, %d, %.2f,"
                        "%.2f, %d, %.2f) "
                "ON DUPLICATE KEY UPDATE time=time;\n",
                current_time,
                state->atmospheric_pressure,

                state->station_sensor.current.temperature,
                state->station_sensor.current.humidity,
                state->station_sensor.current.dew_point,
                state->remote_sensors[0].current.temperature,
                state->remote_sensors[0].current.humidity,
                state->remote_sensors[0].current.dew_point
                );

// The clause "ON DUPLICATE KEY UPDATE time=time" above allows
// to reload the SQL file

        fflush(stream);

}
