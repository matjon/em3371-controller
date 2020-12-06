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
#include <stdio.h>
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

        char *status_file_path;
};

void current_time_to_string(char *time_out, char buffer_size);
int reply_to_ping_packet(int udp_socket, const struct sockaddr_in *packet_source, const unsigned char *received_packet, const size_t received_packet_size);
void dump_incoming_packet(FILE *stream, const struct sockaddr_in *packet_source, const unsigned char *received_packet, const size_t received_packet_size);
