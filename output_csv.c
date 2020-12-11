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

#include "output_csv.h"

void display_CSV_header(FILE *stream)
{
        fputs("time;atmospheric_pressure;"
                "station_temp;station_temp_raw;station_humidity;station_dew_point;"
                "sensor1_temp;sensor1_temp_raw;sensor1_humidity;sensor1_dew_point;"
                "sensor2_temp;sensor2_temp_raw;sensor2_humidity;sensor2_dew_point;"
                "sensor3_temp;sensor3_temp_raw;sensor3_humidity;sensor3_dew_point;"
                "\n", stream);
}

static void display_single_measurement_CSV(FILE *stream, const struct device_single_measurement *state)
{
        if (DEVICE_IS_INCORRECT_TEMPERATURE(state->temperature)) {
                fprintf(stream, ";;");
        } else {
		fprintf(stream, "%.2f;%d;",
			(double) state->temperature, (int) state->raw_temperature
                        );
        }

        if (state->humidity == DEVICE_INCORRECT_HUMIDITY) {
                fprintf(stream, ";");
        } else {
		fprintf(stream, "%d;", (int) state->humidity);
        }

        if (DEVICE_IS_INCORRECT_TEMPERATURE(state->dew_point)) {
                fprintf(stream, ";");
        } else {
		fprintf(stream, "%.2f;", (double) state->dew_point);
        }
}

void display_sensor_state_CSV(FILE *stream, const struct device_sensor_state *state)
{
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time));

        fprintf(stream, "%s;%d;", current_time, state->atmospheric_pressure);
        display_single_measurement_CSV(stream, &(state->station_sensor.current));

        for (int i=0; i < 3; i++) {
                display_single_measurement_CSV(stream, &(state->remote_sensors[i].current));
        }

        fprintf(stream, "\n");

        // When redirecting CSV output to file, there was quite a long delay
        // (even several minutes or more) before the data was actually written
        // to file and accessible.
        //
        // Using fflush manually fixes this.
        // (fflush() does not have undesirable side-effects of fsync())
        fflush(stream);
}
