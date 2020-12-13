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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <math.h>       //isnan(x)
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include "main.h"

struct device_single_measurement {
	float temperature;
	// Raw temperature as sent by the device - for reverse engineering
	uint16_t raw_temperature;
	uint16_t humidity;
        float dew_point;
};

//First two macros are also for dew_point
#define DEVICE_INCORRECT_TEMPERATURE (nan(""))
#define DEVICE_IS_INCORRECT_TEMPERATURE(x) (isnan(x))
#define DEVICE_INCORRECT_HUMIDITY UINT16_MAX

struct device_single_sensor_data {
	bool any_data_present;

	struct device_single_measurement current;

	// One data packet as sent by the device contains three sets of
	// measurements for every sensor.
	// One of them is described in the reverse engineering documentation as
	// containing most recent data, others are described as containing
	// some minimum or maximum values, possibly over some time period.
	//
	// TODO: check how these variables behave.
	struct device_single_measurement historical1;
	struct device_single_measurement historical2;
};

struct device_sensor_state {
	struct device_single_sensor_data station_sensor;
	struct device_single_sensor_data remote_sensors[3];
	// As reported by an internal sensor in the weather station
	uint16_t atmospheric_pressure;

        // tm_wday, tm_yday are not set
        struct tm device_time;
};
#define DEVICE_INCORRECT_PRESSURE UINT16_MAX

void init_logging();
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size,
                const struct program_options *options);
