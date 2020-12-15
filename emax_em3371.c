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
#include "psychrometrics.h"
#include "output_json.h"
#include "output_csv.h"
#include "output_sql.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

/*
 * Using packed structs with casting may be unsafe on some architectures:
 * 	https://stackoverflow.com/questions/8568432/is-gccs-attribute-packed-pragma-pack-unsafe
 * Therefore I'm parsing the packet by hand.
 */

static float fahrenheit_to_celcius(float temperature_fahrenheit)
{
        return (temperature_fahrenheit - 32.) * 5./9.;
}

/*
 * Accesses three bytes from raw_data.
 * Returns true if at least some data is present.
 */
static bool decode_single_measurement(struct device_single_measurement *measurement,
		const unsigned char *raw_data)
{
	if (raw_data[0] == 0xff && raw_data[1] == 0xff && raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		measurement->dew_point = DEVICE_INCORRECT_TEMPERATURE;
		return false;
	}

	if (raw_data[0] == 0xff && raw_data[1] == 0xff) {
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		fprintf(stderr, "Weird: measurement contains humidity, but not temperature.\n");
	} else {
		// Trying to be endianness-agnostic
		uint16_t raw_temperature = raw_data[0] + ((int)raw_data[1]) * 256;
		measurement->raw_temperature = raw_temperature;

		// The device sends temperature in degrees Fahrenheit
                // modified by a linear function.
                //
                // When the device is configured to display temperature in °F,
                // the temperature calculated this way matches exactly the one
                // displayed on the device's screen.
                float temperature_fahrenheit = (float) raw_temperature / 10 - 90;
		measurement->temperature = fahrenheit_to_celcius(temperature_fahrenheit);
	}

	if (raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		fprintf(stderr, "Weird: measurement contains temperature but not humidity.\n");
	} else {
		measurement->humidity = raw_data[2];
	}

        // It would be more elegant to calculate the dew point in functions that
        // handle data presentation, but is done here for performance reasons.
        // It is a time-consuming calculation on devices with software floating
        // point emulation.
        if (measurement->humidity != DEVICE_INCORRECT_HUMIDITY &&
                !DEVICE_IS_INCORRECT_TEMPERATURE(measurement->temperature)) {

                measurement->dew_point =
                        dew_point(measurement->temperature, measurement->humidity);
        } else {
                measurement->dew_point = DEVICE_INCORRECT_TEMPERATURE;
        }

	return true;
}

static bool decode_single_sensor_data(struct device_single_sensor_data *out,
		const unsigned char *raw_data)
{
	bool have_current_data = decode_single_measurement(&(out->current), raw_data);
	bool have_historical1_data = decode_single_measurement(&(out->historical1), raw_data + 3);
	bool have_historical2_data = decode_single_measurement(&(out->historical2), raw_data + 6);

	out->any_data_present = have_current_data || have_historical1_data || have_historical2_data;

	return out->any_data_present;
}

static void decode_sensor_state(struct device_sensor_state *state, const unsigned char *received_packet,
		const size_t received_packet_size)
{
	if (received_packet_size <= 61) {
		fprintf(stderr, "Packet is too short!\n");
		return;
	}

	decode_single_sensor_data(&(state->station_sensor), received_packet + 21);
	for (int i = 0; i < 3; i++) {
		decode_single_sensor_data(&(state->remote_sensors[i]), received_packet + 30 + i*9);
	}

	if (received_packet[59] == 0xff && received_packet[60] == 0xff) {
		state->atmospheric_pressure = DEVICE_INCORRECT_PRESSURE;
	} else {
		state->atmospheric_pressure = received_packet[59] + received_packet[60]*256;
	}

        memset(&(state->device_time), 0, sizeof(state->device_time));
        state->device_time.tm_sec = received_packet[0x13] / 2;
        state->device_time.tm_min = received_packet[0x12];
        state->device_time.tm_hour = received_packet[0x11];
        state->device_time.tm_mday = received_packet[0x10];
        state->device_time.tm_mon = received_packet[0x0f]-1;
        state->device_time.tm_year = received_packet[0x0e]+100;
        state->device_time.tm_isdst = -1; // information not available
}

static bool is_packet_correct(const unsigned char *received_packet,
		const size_t received_packet_size)
{
        if (received_packet[0] != '<') {
                fprintf(stderr, "Incorrect first byte of packet\n");
                return false;
        }

        if (received_packet[received_packet_size-1] != '>') {
                fprintf(stderr, "Incorrect last byte of packet\n");
                return false;
        }

        unsigned char sum = 0;
        for (size_t i=0; i < received_packet_size-2; i++) {
                sum+=received_packet[i];
        }
        if (received_packet[received_packet_size-2] != sum) {
                fprintf(stderr, "Incorrect packet checksum\n");
                return false;
        }
        fprintf(stderr, "Packet appears valid\n");
        return true;
}



void init_logging()
{
        if (nan("") == 0 || !isnan(nan(""))) {
                //Are there any embedded architectures without support for NaN?
                //See:
                //      https://stackoverflow.com/q/2234468
                //      https://www.gnu.org/software/libc/manual/html_node/Infinity-and-NaN.html
                //              [...] available only on machines that support the “not a number”
                //              value—that is to say, on all machines that support IEEE floating
                //              point.
                fprintf(stderr, "ERROR: no support for NaN floating point numbers "
                                "in hardware or software floating point library.\n");
                exit(1);
        }

        fprintf(stderr, "Warning: output formats are subject to change\n");
        display_CSV_header(stdout);
}

static void update_status_file(const struct program_options *options,
                struct device_sensor_state *sensor_state)
{
        if (options->status_file_path == NULL) {
                return;
        }

        FILE *status_file = fopen(options->status_file_path, "w");

        if (status_file == NULL) {
                perror("Cannot open status file for writing");
                return;
        }

        display_sensor_state_json(status_file, sensor_state);

        fclose(status_file);
}

// Main program logic
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size,
                const struct program_options *options)
{
	dump_incoming_packet(stderr, packet_source, received_packet, received_packet_size);
        if (!is_packet_correct(received_packet, received_packet_size)) {
                return;
        }

	if (received_packet_size < 20) {
                if (options->reply_to_ping_packets) {
                        fputs("Handling the received packet "
                                "as a ping packet, sending it back\n", stderr);
                        send_udp_packet(udp_socket, packet_source,
                                        received_packet, received_packet_size);
                }
	} else if (received_packet_size >= 65) {

		struct device_sensor_state *sensor_state;
		sensor_state = malloc(sizeof(struct device_sensor_state));
                if (sensor_state == NULL){
		        fprintf(stderr, "process_incoming_packet: Cannot allocate memory!\n");
                        return;
                }

		decode_sensor_state(sensor_state, received_packet, received_packet_size);
		display_sensor_state_json(stderr, sensor_state);
		display_sensor_state_CSV(stdout, sensor_state);
                update_status_file(options, sensor_state);

		free(sensor_state);
	}
}
