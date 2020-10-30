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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>

#define RECEIVE_PACKET_SIZE 1500

int reply_to_ping_packet(int udp_socket, const struct sockaddr_in *packet_source, const unsigned char *received_packet, const size_t received_packet_size);
void dump_incoming_packet(FILE *stream, const struct sockaddr_in *packet_source, const unsigned char *received_packet, const size_t received_packet_size);
