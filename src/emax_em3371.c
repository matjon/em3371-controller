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

// tm_gmtoff in struct tm
#define _GNU_SOURCE

// localtime_r
#define _POSIX_C_SOURCE 200809L

#include "emax_em3371.h"
#include "main.h"
#include "psychrometrics.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

//fcntl
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>


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

        state->payload_byte_0x31 = received_packet[61];

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

void init_device_logic(struct program_options *options)
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

        if (options->allow_injecting_packets) {
                int current_flags = fcntl(0, F_GETFL);
                fcntl(0, F_SETFL, current_flags | O_NONBLOCK);
        }
}

static void send_timesync_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size)
{
        if (received_packet_size <= 8) {
                // TODO: print a warning message
                return;
        }

        unsigned char buffer[22];

        memset(buffer, 0, sizeof(buffer));
        // First 7 bytes contain preamble and last 4 bytes of device's MAC address
        memcpy(buffer, received_packet, 7);
        // Setting this to 0x40 causes the device to respond with an error message
        // same with 0x81
        buffer[7] = 0x80;
        //buffer[8] = 0x10;
        buffer[8] = 0x00;
        // Other values then 0x01 seem uninteresting
        //buffer[9] = 0x00;
        //buffer[9] = 0x03;
        buffer[9] = 0x01;
        //setting this to 0x10 or 0x08, or 0x07 causes the device to respond with an error message
        //buffer[10] = 0x04;
        //buffer[10] = 0x10;
        //buffer[10] = 0x07;
        buffer[10] = 0x08;
        //Setting this byte to 0x01 does apparently nothing, the device sets this byte to 0x00 in reply
        //buffer[11] = 0x01;
        buffer[11] = 0x00;

        time_t current_time = time(NULL);
        struct tm current_time_tm;
        localtime_r(&current_time, &current_time_tm);

        // tm_gmtoff is a GNU extension, but also available in uclibc
        int current_timezone = current_time_tm.tm_gmtoff / 3600;

        // paranoia
        if (current_timezone < -12) {
                current_timezone = -12;
        }
        if (current_timezone > 12) {
                current_timezone = 12;
        }

        // GMT is represented by timezone 0x0c (12 in decimal)
        buffer[12] = 12 + current_timezone;
        //buffer[12] = 5;

        buffer[13] = current_time_tm.tm_year - 100;
        buffer[14] = current_time_tm.tm_mon + 1;
        buffer[15] = current_time_tm.tm_mday;

        buffer[16] = current_time_tm.tm_hour;
        buffer[17] = current_time_tm.tm_min;
        buffer[18] = current_time_tm.tm_sec * 2;
        //Setting this to 0x01 does apparently nothing interesting
        //buffer[19] = 0x01;
        buffer[19] = 0x00;

        unsigned char sum = 0;
        for (size_t i=0; i < 20; i++) {
                sum+=buffer[i];
        }
        buffer[20] = sum;
        buffer[21] = '>';

	dump_packet(stderr, packet_source, buffer, sizeof(buffer), false);
        send_udp_packet(udp_socket, packet_source, buffer, sizeof(buffer));
}

unsigned char decode_hex_digit(char digit)
{
        digit = tolower(digit);
        if (isdigit(digit)) {
                return digit-'0';
        } else {
                return digit-'a' + 10;
        }
}

bool decode_hex(const char *buffer, unsigned char *out) {
        if (!isxdigit(*buffer) || *(buffer+1) == 0 || !isxdigit(*(buffer+1))) {
                return false;
        }

        *out = decode_hex_digit(*buffer) << 4;
        *out += decode_hex_digit(*(buffer+1));
        return true;
}

static bool inject_packets(int udp_socket, const struct sockaddr_in *packet_source)
{
        char hex_buffer[200];
        char *ret = fgets(hex_buffer, sizeof(hex_buffer), stdin);
        if (ret == NULL) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return false;
                } else {
                        perror("Reading packets to inject failed!");
                        return false;
                }
        }

        unsigned char output_buffer[100];
        size_t output_buffer_size = 0;
        char *hex_buffer_ptr=hex_buffer;

        while (*hex_buffer_ptr != 0 && output_buffer_size < sizeof(output_buffer)) {
                if (!isxdigit(*hex_buffer_ptr)) {
                        hex_buffer_ptr++;
                } else if (decode_hex(hex_buffer_ptr, &output_buffer[output_buffer_size])) {
                        output_buffer_size++;
                        hex_buffer_ptr+=2;
                } else {
                        hex_buffer_ptr++;
                }
        }
        if (output_buffer_size == 0) {
                return false;
        }

        if (output_buffer_size + 2 > sizeof(output_buffer)) {
                fprintf(stderr, "Packet to inject too big\n");
                return false;
        }

        unsigned char sum = 0;
        for (size_t i=0; i < output_buffer_size; i++) {
                sum+=output_buffer[i];
        }
        output_buffer[output_buffer_size++] = sum;
        output_buffer[output_buffer_size++] = '>';


        fprintf(stderr, "Injecting packet:\n");
	dump_packet(stderr, packet_source, output_buffer, output_buffer_size, false);
        send_udp_packet(udp_socket, packet_source, output_buffer, output_buffer_size);

        return true;
}

static unsigned char fuzzing_function_no = 0x03;

void fuzz_station(int udp_socket, const struct sockaddr_in *packet_source,
		unsigned char *received_packet, const size_t received_packet_size)
{
        unsigned char output_buffer[300];
        size_t output_buffer_size = 0;

        // First 7 bytes contain preamble and last 4 bytes of device's MAC address
        if (received_packet_size <= 8) {
                // TODO: print a warning message
                return;
        }
        memcpy(output_buffer, received_packet, 7);
        output_buffer_size=7;

        output_buffer[output_buffer_size++] = fuzzing_function_no++;
        output_buffer[output_buffer_size++] = 0x00;
        output_buffer[output_buffer_size++] = 0x00;

        // payload length
        output_buffer[output_buffer_size++] = 0x0e;
        output_buffer[output_buffer_size++] = 0x00;

        for (unsigned char i = 0; i < 0x0e; i++) {
                output_buffer[output_buffer_size++] = (unsigned char) i;
        }


        unsigned char sum = 0;
        for (size_t i=0; i < output_buffer_size; i++) {
                sum+=output_buffer[i];
        }
        output_buffer[output_buffer_size++] = sum;
        output_buffer[output_buffer_size++] = '>';

        fprintf(stderr, "Injecting fuzzing packet:\n");
	dump_packet(stderr, packet_source, output_buffer, output_buffer_size, false);
        send_udp_packet(udp_socket, packet_source, output_buffer, output_buffer_size);
}


static int has_timesync_packet_been_sent = 0;

// Main program logic
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size,
                const struct program_options *options)
{
	dump_packet(stderr, packet_source, received_packet, received_packet_size, true);
        if (!is_packet_correct(received_packet, received_packet_size)) {
                return;
        }

	if (received_packet_size < 20
                        && received_packet_size > 0x07
                        && received_packet[0x07] <= 0x01) {
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

                if (options->allow_injecting_packets) {
                        while (inject_packets(udp_socket, packet_source)) {
                                sleep(1);
                        }
                        //fuzz_station(udp_socket, packet_source);
                }

                        if (!has_timesync_packet_been_sent) {
                                fputs("Injecting timesync data into the device\n", stderr);
                                send_timesync_packet(udp_socket, packet_source,
                                                received_packet, received_packet_size);
                                has_timesync_packet_been_sent = 1;
                        }
	}
}
