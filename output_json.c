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


#include "emax_em3371.h"
#include "main.h"
#include "output_json.h"
#include <stdio.h>

static void display_single_measurement_json(FILE *stream, const struct device_single_measurement *state)
{
	fprintf(stream, "{");

	if (!DEVICE_IS_INCORRECT_TEMPERATURE(state->temperature)) {
		fprintf(stream, " \"temperature\": %.2f, \"raw_temperature\": %d",
			(double) state->temperature, (int) state->raw_temperature);

                if (state->humidity != DEVICE_INCORRECT_HUMIDITY) {
                        fputs(",", stream);
                }
	}

	if (state->humidity != DEVICE_INCORRECT_HUMIDITY) {
		fprintf(stream, " \"humidity\": %d", (int) state->humidity);
	}

        if (!DEVICE_IS_INCORRECT_TEMPERATURE(state->dew_point)) {
		fprintf(stream, ", \"dew_point\": %.2f", (double) state->dew_point);
        }

	fprintf(stream, " }");
}

static void display_single_sensor_json(FILE *stream, const struct device_single_sensor_data *state)
{
	fprintf(stream, "{\n");

	fprintf(stream, "    \"current\": ");
	display_single_measurement_json(stream, &(state->current));

	fprintf(stream, ",\n    \"historical1\": ");
	display_single_measurement_json(stream, &(state->historical1));

	fprintf(stream, ",\n    \"historical2\": ");
	display_single_measurement_json(stream, &(state->historical2));

	fprintf(stream, "\n  }");
}

void display_sensor_state_json(FILE *stream, const struct device_sensor_state *state)
{
	fputs("{ ", stream);

	char device_time_str[30];
	tm_to_string(&(state->device_time), device_time_str, sizeof(device_time_str));

        fprintf(stream, "\"device_time\": \"%s\",\n", device_time_str);
	if (state->atmospheric_pressure != DEVICE_INCORRECT_PRESSURE) {
		fprintf(stream, "\"atmospheric_pressure\": %u,\n", state->atmospheric_pressure);
	}
	fprintf(stream, "  \"station_sensor\": ");

	display_single_sensor_json(stream, &(state->station_sensor));

	for (int i = 0; i <= 2; i++) {
                if (state->remote_sensors[i].any_data_present) {
                        // The sensors are not interchangeable.
                        // Therefore I'm not using an array, but a key-value table.
                        fprintf(stream, ",\n  \"sensor%d\": ", i+1);
                        display_single_sensor_json(stream, &(state->remote_sensors[i]));
                }
	}

	fprintf(stream, "\n}\n");
}
