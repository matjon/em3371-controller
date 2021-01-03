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

#include "output_csv.h"
#include <string.h>

static FILE *csv_output_stream = NULL;
static bool csv_output_stream_close_on_exit = false;

static void display_CSV_header(FILE *stream)
{
        fputs("time;atmospheric_pressure;"
                "station_temp;station_humidity;station_dew_point;"
                "sensor1_temp;sensor1_humidity;sensor1_dew_point;"
                "sensor2_temp;sensor2_humidity;sensor2_dew_point;"
                "sensor3_temp;sensor3_humidity;sensor3_dew_point;"
                "\n", stream);
        fflush(stream);
}

bool init_CSV_output(const char *csv_output_path)
{
        bool ret = open_output_file(csv_output_path, &csv_output_stream,
                        &csv_output_stream_close_on_exit, "CSV");

        if (ret) {
                display_CSV_header(csv_output_stream);
        }
        return ret;
}

void shutdown_CSV_output()
{
        close_output_file(&csv_output_stream, &csv_output_stream_close_on_exit);
}

static void display_single_measurement_CSV(FILE *stream, const struct device_single_measurement *state)
{
        if (DEVICE_IS_INCORRECT_TEMPERATURE(state->temperature)) {
                fprintf(stream, ";");
        } else {
		fprintf(stream, "%.2f;", (double) state->temperature);
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

void display_sensor_state_CSV(const struct device_sensor_state *state)
{
        FILE *stream = csv_output_stream;

	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time), true);

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
