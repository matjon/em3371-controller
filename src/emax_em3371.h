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

#pragma once

struct device_sensor_state;
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <math.h>       //isnan(x)
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>

struct device_single_measurement {
	float temperature;
	uint16_t humidity;
        float dew_point;
};

//First two macros are also for dew_point
#define DEVICE_INCORRECT_TEMPERATURE (nan(""))
#define DEVICE_IS_INCORRECT_TEMPERATURE(x) (isnan(x))
#define DEVICE_INCORRECT_HUMIDITY UINT16_MAX

struct device_single_sensor_data {
	bool any_data_present;
        bool battery_low;

	struct device_single_measurement current;

	// One data packet as sent by the device contains three sets of
	// measurements for every sensor.
	// One of them is described in the reverse engineering documentation as
	// containing most recent data, others are described as containing
	// some minimum or maximum values, possibly over some time period.
	//
	// TODO: check how these variables behave.
	struct device_single_measurement historical_max;
	struct device_single_measurement historical_min;
};

struct device_sensor_state {
	struct device_single_sensor_data station_sensor;
	struct device_single_sensor_data remote_sensors[3];
	// As reported by an internal sensor in the weather station
	uint16_t atmospheric_pressure;
        // Testing: byte 0x31 of payload may contain atmospheric pressure trend.
        unsigned char payload_byte_0x31;

        struct tm device_time;
};
#define DEVICE_INCORRECT_PRESSURE UINT16_MAX

void init_device_logic(struct program_options *options);
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size,
                const struct program_options *options);

void fuzz_station(int udp_socket, const struct sockaddr_in *packet_source,
		unsigned char *received_packet, const size_t received_packet_size);
