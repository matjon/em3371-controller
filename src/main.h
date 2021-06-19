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

struct program_options;
#include "emax_em3371.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>

#define RECEIVE_PACKET_SIZE 1500

struct program_options {
        // in host byte order
	struct in_addr bind_address;
        // in host byte order
        uint16_t bind_port;

        bool reply_to_ping_packets;
        bool allow_injecting_packets;

        char *csv_output_path;
        char *raw_sql_output_path;
        char *status_file_path;

#ifdef HAVE_MYSQL
        char *mysql_server;
        char *mysql_user;
        char *mysql_password;
        char *mysql_database;

        size_t mysql_buffer_size;
#endif
};
#define DEFAULT_BIND_PORT 17000

void time_to_string(const time_t time_in, char *time_out,
                const size_t buffer_size, bool use_localtime);

void current_time_to_string(char *time_out, const size_t buffer_size, bool apply_timezone);

bool open_output_file(const char *output_path, FILE **output_stream,
                bool *output_close_on_exit, const char *output_type_name);
void close_output_file(FILE **output_stream, bool *output_close_on_exit);

int send_udp_packet(int udp_socket, const struct sockaddr_in *packet_source, const unsigned char *received_packet, const size_t received_packet_size);
void dump_packet(FILE *stream, const struct sockaddr_in *packet_source,
        const unsigned char *received_packet, const size_t received_packet_size,
        bool is_incoming);
void handle_decoded_sensor_state(const struct device_sensor_state *sensor_state,
                const struct program_options *options);
