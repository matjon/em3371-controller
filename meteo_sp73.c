
#include "meteo_sp73.h"
#include "main.h"
#include "config.h"

#include <stdlib.h>

// Main program logic
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size)
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
