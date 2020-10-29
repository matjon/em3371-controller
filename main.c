/*
    Copyright (C) 2020 Mateusz Jończyk

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#define _POSIX_C_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "config.h"

#define RECEIVE_PACKET_SIZE 1500


// May be thread-unsafe because I use inet_ntoa
// Returned value must be disposed of by free()
static char *packet_source_to_string(const struct sockaddr_in *packet_source)
{
	char *packet_source_string = NULL;
	int ret = 0;

	if (packet_source->sin_family != AF_INET) {
		ret = asprintf( &packet_source_string, "(weird src_addr family %ld)", (long int) packet_source->sin_family);
	} else {
		char *source_ip = inet_ntoa(packet_source->sin_addr);
		uint16_t source_port = ntohs(packet_source->sin_port);

		ret = asprintf( &packet_source_string, "%s:%d", source_ip, (int) source_port);
	}

	if (ret == -1)
		return NULL;

	return packet_source_string;
}

// It is advisable to call tzset() at the beginning of program
// Displays a local time converted to a string
void current_time_to_string(char *time_out, char buffer_size)
{
	time_t current_time = time(NULL);
	struct tm current_time_tm;

	localtime_r(&current_time, &current_time_tm);

	// Time should be in a format that LibreOffice is able to understand
	strftime(time_out, buffer_size, "%Y-%m-%d %H:%M:%S", &current_time_tm);
}

void hexdump_buffer(FILE *stream, const char *buffer, size_t buffer_size, const int bytes_per_line)
{
	// TODO: make the function more readable
	// TODO: replace fprintf with something faster, like sprintf
	for (size_t i = 0; i < buffer_size; i+=bytes_per_line) {
		fprintf(stream, "%08x  ", (unsigned int) i);

		for (size_t j = i; j < i + bytes_per_line && j < buffer_size; j++) {
			fprintf(stream, "%02x ", (unsigned int) buffer[j]);
		}
		// last line padding
		for (size_t j = buffer_size; j < i + bytes_per_line; j++) {
			fputs("   ", stream);
		}

		fprintf(stream, " |");
		for (size_t j = i; j < i+bytes_per_line && j < buffer_size; j++) {
			if (isprint(buffer[j])) {
				fputc(buffer[j], stream);
			} else {
				fputc('.', stream);
			}
		}
		// last line padding
		for (size_t j = buffer_size; j < i+bytes_per_line; j++) {
			fputs(" ", stream);
		}
		fprintf(stream, "|\n");
	}
}

static void dump_incoming_packet(FILE *stream, const struct sockaddr_in *packet_source, const char *received_packet, const size_t received_packet_size)
{
	char *packet_source_text = packet_source_to_string(packet_source);
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time));

	fprintf(stream, "%s received a packet from %s :\n", current_time, packet_source_text);

	hexdump_buffer(stream, received_packet, received_packet_size, 8);
	free(packet_source_text);
}


static int reply_to_ping_packet(int udp_socket, const struct sockaddr_in *packet_source, const char *received_packet, const size_t received_packet_size)
{
	long int ret = 0;
	fputs("Handling the received packet as a ping packet, sending it back\n", stderr);
	ret = sendto(udp_socket, received_packet, received_packet_size, 0,
			(const struct sockaddr*) packet_source, sizeof(*packet_source));

	if (ret != (long int)received_packet_size) {
		if (ret == -1) {
			perror("Cannot reply to a ping packet");
		} else {
			fprintf(stderr, "Problem with reply to a ping packet: tried to send %ld bytes, sent %ld bytes.\n", (long int) received_packet_size, ret);
		}
		return -1;
	}
	return 0;
}


// Implements main program logic
static void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source, const char *received_packet, const size_t received_packet_size)
{

	dump_incoming_packet(stderr, packet_source, received_packet, received_packet_size);

#if defined(CONFIG_REPLY_TO_PING_PACKETS) && (CONFIG_REPLY_TO_PING_PACKETS == 1)
	// See description in config.h
	// TODO: check the contents of the packet, not just its size
	if (received_packet_size < 20) {
		reply_to_ping_packet(udp_socket, packet_source, received_packet, received_packet_size);
	}
#endif
}


int main()
{
	tzset();

	int ret = 0;

	int udp_socket = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (udp_socket == -1) {
		perror("Cannot create socket");
		abort();
	}

	struct in_addr bind_address;
	if (! inet_aton(CONFIG_BIND_ADDRESS, &bind_address)) {
		puts("Incorrect CONFIG_BIND_ADDRESS defined in config.h\n");
		abort();
	}
	struct sockaddr_in bind_sockaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(CONFIG_BIND_PORT),
		.sin_addr = bind_address,
	};

	ret = bind(udp_socket, (struct sockaddr *) &bind_sockaddr, sizeof(bind_sockaddr));
	if (ret == -1) {
		perror("Cannot bind socket");
		abort();
	}

	char *received_packet = malloc(RECEIVE_PACKET_SIZE);
	if (received_packet == NULL) {
		perror("Cannot allocate memory");
		abort();
	}

	// TODO: implement proper program termination
	while (1) {
		struct sockaddr_in src_addr;
		socklen_t src_addr_size;

		/* man socket:
		 * SOCK_DGRAM  and  SOCK_RAW sockets allow sending of datagrams to
		 * correspondents named in sendto(2) calls.  Datagrams are generally
		 * received with recvfrom(2), which returns the next datagram along
		 * with the address of its sender.
		 */

		src_addr_size = sizeof(src_addr);
		ret = recvfrom(udp_socket, received_packet, RECEIVE_PACKET_SIZE, 0,
                        (struct sockaddr *) &src_addr, &src_addr_size);

		if (ret == -1) {
			perror("recvfrom failed");
			// We continue anyway
		} else {
			process_incoming_packet(udp_socket, &src_addr, received_packet, ret);
		}
	}

	free(received_packet);
	close(udp_socket);

	return 0;
}

