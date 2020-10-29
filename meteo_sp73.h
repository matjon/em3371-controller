#pragma once

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>

void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source, 
		const unsigned char *received_packet, const size_t received_packet_size);
