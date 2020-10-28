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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "config.h"

#define RECEIVE_PACKET_SIZE 1500

// TODO: use malloc()
char received_packet[RECEIVE_PACKET_SIZE];

int main()
{
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

	while (1) {
		struct sockaddr_in src_addr;
		socklen_t src_addr_size;

		src_addr_size = sizeof(src_addr);

		/* man socket:
		 * SOCK_DGRAM  and  SOCK_RAW sockets allow sending of datagrams to
		 * correspondents named in sendto(2) calls.  Datagrams are generally
		 * received with recvfrom(2), which returns the next datagram along
		 * with the address of its sender.
		 */
		ret = recvfrom(udp_socket, received_packet, sizeof(received_packet), 0,
                        (struct sockaddr *) &src_addr, &src_addr_size);

		// TODO: remember to check value of ret

		fwrite(received_packet, ret, 1, stdout);
	}

	close(udp_socket);

	return 0;
}

