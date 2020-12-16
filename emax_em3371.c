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

#include <assert.h>
#include <stdbool.h>
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
		const unsigned char *raw_data,
                bool calculate_dew_point)
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
        //
        // Calculating dew point for min/max measurements does not make
        // sense as min humidity and min temperature do not correspond to each
        // other (they were not taken at the same time).
        if (measurement->humidity != DEVICE_INCORRECT_HUMIDITY
                && !DEVICE_IS_INCORRECT_TEMPERATURE(measurement->temperature)
                && calculate_dew_point) {

                measurement->dew_point =
                        dew_point(measurement->temperature, measurement->humidity);
        } else {
                measurement->dew_point = DEVICE_INCORRECT_TEMPERATURE;
        }

	return true;
}

static void check_measurement_ordering(
        struct device_single_measurement *smaller,
        struct device_single_measurement *bigger,
        const char *smaller_description,
        const char *bigger_description
        )
{
        const float temperature_epsilon = 0.01;
        if (!DEVICE_IS_INCORRECT_TEMPERATURE(smaller->temperature)
                && !DEVICE_IS_INCORRECT_TEMPERATURE(bigger->temperature)) {
                if (smaller->temperature > bigger->temperature + temperature_epsilon) {
                        fprintf(stderr,
                                "Warning: %s temperature (%.2f°C) is bigger "
                                "then %s temperature (%.2f°C).\n",
                                smaller_description, (double) smaller->temperature,
                                bigger_description, (double) bigger->temperature
                                );
                }
        }

        if (smaller->humidity != DEVICE_INCORRECT_HUMIDITY
                && bigger->humidity != DEVICE_INCORRECT_HUMIDITY) {
                if (smaller->humidity > bigger->humidity) {
                        fprintf(stderr,
                                "Warning: %s humidity (%d%%)is bigger "
                                "then %s humidity (%d%%).\n",
                                smaller_description, (int) smaller->humidity,
                                bigger_description, (int) bigger->humidity);
                }
        }
}

static bool decode_single_sensor_data(struct device_single_sensor_data *out,
		const unsigned char *raw_data)
{
	bool have_current_data =
                decode_single_measurement(&(out->current),        raw_data, true);
	bool have_historical_max_data =
                decode_single_measurement(&(out->historical_max), raw_data + 3, false);
	bool have_historical_min_data =
                decode_single_measurement(&(out->historical_min), raw_data + 6, false);

	out->any_data_present =
                have_current_data || have_historical_max_data || have_historical_min_data;

        check_measurement_ordering(&out->historical_min, &out->current,
                        "historical minimum", "current");
        check_measurement_ordering(&out->current, &out->historical_max,
                        "current", "historical maximum");

	return out->any_data_present;
}

static void decode_device_time(struct device_sensor_state *state,
                const unsigned char *received_packet,
		const size_t received_packet_size)
{
        if (received_packet_size <= 0x13) {
                assert(false);
                return;
        }

        memset(&(state->device_time), 0, sizeof(state->device_time));
        state->device_time.tm_sec = received_packet[0x13] / 2;
        state->device_time.tm_min = received_packet[0x12];
        state->device_time.tm_hour = received_packet[0x11];
        state->device_time.tm_mday = received_packet[0x10];
        state->device_time.tm_mon = received_packet[0x0f]-1;
        state->device_time.tm_year = received_packet[0x0e]+100;
        state->device_time.tm_isdst = -1; // information not available


        // I received a date "2020-12-15 10:60:00" instead of the expected
        // "2020-12-15 11:00:00".
        // This caused a SQL error when inserting into a DATETIME field.
        //
        // mktime() should correct this, according to the documentation.

        // If the second part is equal to 60, mktime() may handle this as a leap
        // second, which would be an error (the weather station most probably
        // does not handle leap seconds).
        // Therefore I'm correcting it here manually.
        if (state->device_time.tm_sec >= 60) {
                state->device_time.tm_min++;
                state->device_time.tm_sec -= 60;
        }
        mktime(&state->device_time);
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

        decode_device_time(state, received_packet, received_packet_size);
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

void init_device_logic()
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
                handle_decoded_sensor_state(sensor_state, options);
		free(sensor_state);
	}
}
