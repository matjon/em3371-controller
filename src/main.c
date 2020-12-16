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

#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "emax_em3371.h"
#include "output_json.h"
#include "output_csv.h"
#include "output_sql.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
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
#include <getopt.h>


volatile bool stop_execution = false;
volatile int stop_execution_signal = 0;

// Returned value must be disposed of by free()
static char *packet_source_to_string(const struct sockaddr_in *packet_source)
{
        const size_t packet_source_string_size = 50;
	char *packet_source_string = malloc(packet_source_string_size);
        if (packet_source_string == NULL) {
                return NULL;
        }

	int ret = 0;

	if (packet_source->sin_family != AF_INET) {
		ret = snprintf(packet_source_string,
                               packet_source_string_size,
                               "(weird src_addr family %ld)",
                               (long int) packet_source->sin_family);
	} else {
                char source_ip[INET_ADDRSTRLEN];

                const char *inet_ret = inet_ntop(AF_INET,
                                &packet_source->sin_addr,
                                source_ip,
                                sizeof(source_ip));

                if (inet_ret == NULL) {
                        perror("Cannot convert source IP address to string");
                        goto err;
                }

		uint16_t source_port = ntohs(packet_source->sin_port);

		ret = snprintf(packet_source_string,
                               packet_source_string_size,
                               "%s:%d", source_ip, (int) source_port);
	}

	if (ret == -1) {
                goto err;
        }

	return packet_source_string;

err:
        free(packet_source_string);
        return NULL;
}

static void initialize_TZ_env()
{
        char tzfile_contents[100];
        FILE *tzfile;
        char *tzfile_newline = NULL;

        tzfile = fopen("/etc/TZ", "r");
        if (tzfile == NULL) {
                return;
        }

        if (fgets(tzfile_contents, sizeof(tzfile_contents), tzfile) == NULL) {
                goto close;
        }

        tzfile_newline = strchr(tzfile_contents, '\n');
        if (tzfile_newline != NULL) {
                *tzfile_newline = '\0';
        }

        // paranoia
        tzfile_newline = strchr(tzfile_contents, '\r');
        if (tzfile_newline != NULL) {
                *tzfile_newline = '\0';
        }

        setenv("TZ", tzfile_contents, 0);

close:
        fclose(tzfile);
}

static void initialize_timezone()
{
        /*
         * My DD-WRT router uses uClibc, which expects timezone to be specified
         * in the "TZ" environment variable (see "man tzset").
         *      https://www.uclibc.org/FAQ.html#timezones
         * The contents from this environment variable are to be initialized
         * from /etc/TZ, but DD-WRT shell seems not to do this.
         * uClibc can probably be configured to automatically read timezone
         * configuration from /etc/TZ, but it seems it is not.
         *
         * Therefore we set the TZ environment variable manually before calling
         * tzset(). On normal Linux /etc/TZ would not exist in most cases, so
         * this has no effect then.
         */
        if (getenv("TZ") == NULL) {
                initialize_TZ_env();
        }

	tzset();
}

void tm_to_string(const struct tm *time_tm, char *time_out,
                const size_t buffer_size)
{
	// Time should be in a format that LibreOffice is able to understand
	strftime(time_out, buffer_size, "%Y-%m-%d %H:%M:%S", time_tm);
}

// Displays current time converted to a string - either local time or time in
// GMT, depending on the apply_timezone parameter.
// Call initialize_timezone() once before using this function.
void current_time_to_string(char *time_out, const size_t buffer_size,
                bool apply_timezone)
{
	time_t current_time = time(NULL);
	struct tm current_time_tm;

        if (apply_timezone) {
                localtime_r(&current_time, &current_time_tm);
        } else {
                gmtime_r(&current_time, &current_time_tm);
        }

        tm_to_string(&current_time_tm, time_out, buffer_size);
}


static void hexdump_buffer(FILE *stream, const unsigned char *buffer,
                size_t buffer_size, const int bytes_per_line)
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

void dump_incoming_packet(FILE *stream, const struct sockaddr_in *packet_source,
                const unsigned char *received_packet, const size_t received_packet_size)
{
	char *packet_source_text = packet_source_to_string(packet_source);
	char current_time[30];
	current_time_to_string(current_time, sizeof(current_time), true);

	fprintf(stream, "%s received a packet from %s :\n", current_time, packet_source_text);

	hexdump_buffer(stream, received_packet, received_packet_size, 8);
	free(packet_source_text);
}


int send_udp_packet(int udp_socket, const struct sockaddr_in *destination,
                const unsigned char *payload, const size_t payload_size)
{
	long int ret = 0;
	ret = sendto(udp_socket, payload, payload_size, 0,
			(const struct sockaddr*) destination, sizeof(*destination));

	if (ret != (long int)payload_size) {
		if (ret == -1) {
			perror("Cannot send an UDP packet:");
		} else {
			fprintf(stderr, "Problem with sending an UDP packet: "
                                        "tried to send %ld bytes, sent %ld bytes.\n",
                                        (long int) payload_size, ret
                                );
		}
		return -1;
	}
	return 0;
}

static void init_logging()
{
        fprintf(stderr, "Warning: output formats are subject to change\n");
        display_CSV_header(stdout);
}

void handle_decoded_sensor_state(const struct device_sensor_state *sensor_state,
                const struct program_options *options)
{
        display_sensor_state_json(stderr, sensor_state);
        display_sensor_state_CSV(stdout, sensor_state);
        // display_sensor_state_sql(stdout, sensor_state);
        update_status_file(options->status_file_path, sensor_state);
}

static void on_interrupt(int signum)
{
        stop_execution = true;
        stop_execution_signal = signum;
}

static void init_signals()
{
        sigset_t signal_mask;
        sigemptyset(&signal_mask);

        struct sigaction signal_action = {
                .sa_handler = on_interrupt,
                .sa_mask = signal_mask,
                .sa_flags = 0,
                .sa_restorer = NULL,
        };

#define INSTALL_SIGNAL(signame) \
        if (sigaction(signame, &signal_action, NULL) != 0) {            \
                perror("sigaction failed for " #signame );              \
        }

        INSTALL_SIGNAL(SIGTERM)
        INSTALL_SIGNAL(SIGHUP)
        INSTALL_SIGNAL(SIGINT)
}

static void parse_program_options(const int argc, char **argv,
                struct program_options *options)
{
        int ret;

        static struct option long_options[] = {
                { "bind-address", required_argument, NULL, 'a' },
                { "port",         required_argument, NULL, 'p' },
                { "no-reply",     no_argument,       NULL, 'r' },
                { "status-file",  required_argument, NULL, 's' },
                {0, 0, 0, 0}
        };

        // Set defaults
        inet_pton(AF_INET, "0.0.0.0", &(options->bind_address));
        options->bind_port = DEFAULT_BIND_PORT;

        options->reply_to_ping_packets = true;
        options->status_file_path = NULL;

        // The following is vaguely based on the example code in
        // https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html
        while (true) {
                int option_index = 0;
                char *endptr = NULL;
                long port_number = 0;

                ret = getopt_long (argc, argv, "a:p:rs:", long_options, &option_index);
                if (ret == -1) {
                        break;
                }

                switch (ret) {
                case 'a':
                        if (inet_pton(AF_INET, optarg, &(options->bind_address)) == 0) {
                                fputs("Incorrect bind address specified on command line!\n",
                                                stderr);
                                exit(1);
                        }
                        break;

                case 'p':
                        endptr = NULL;
                        port_number = strtol(optarg, &endptr, 10);
                        if (*endptr != 0) {
                                fputs("Incorrect listen port specified on command line!\n",
                                                stderr);
                                exit(1);
                        }
                        if (port_number > 65535 || port_number <= 0 ) {
                                fputs("Listen port passed on command line out of range!\n",
                                                stderr);
                                exit(1);
                        }
                        options->bind_port = port_number;
                        break;

                case 'r':
                        /*
                        * The Meteo SP73 weather station sends two kinds of packets: short and long ones.
                        *
                        * The long packets contain proper sensor data, the short packets appear to be part of
                        * a ping-like mechanism: it is probably necessary to send them back
                        * so that the weather station will send long packets later.
                        *
                        * This variable controls whether to send short ping packets back.
                        * It should normally be enabled (defined), except in cases
                        * when packets are duplicated to another computer using
                        * e.g. the TEE target from iptables.
                        * In this case, this knob should be enabled only on one of the computers
                        * so that the weather station would not be confused.
                        */
                        options->reply_to_ping_packets = false;
                        break;

                case 's':
                        options->status_file_path = optarg;
                        break;

                case '?':
                        exit(1);
                default:
                        fputs("Incorrect command line parameters!\n", stderr);
                        exit(1);
                }
        }

        if (optind < argc) {
                fputs("Incorrect command line parameters!\n", stderr);
                exit(1);
        }
}

int main(int argc, char **argv)
{
        initialize_timezone();

        struct program_options options;
        parse_program_options(argc, argv, &options);

	int ret = 0;

        // TODO: set SOCK_CLOEXEC. Setting in in call to socket() does not
        // work on some DD-WRT routers.
	int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket == -1) {
		perror("Cannot create socket");
		exit(1);
	}

	struct sockaddr_in bind_sockaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(options.bind_port),
		.sin_addr = options.bind_address,
	};

	ret = bind(udp_socket, (struct sockaddr *) &bind_sockaddr, sizeof(bind_sockaddr));
	if (ret == -1) {
		perror("Cannot bind socket");
		exit(1);
	}

	unsigned char *received_packet = malloc(RECEIVE_PACKET_SIZE);
	if (received_packet == NULL) {
		perror("Cannot allocate memory");
		exit(1);
	}

        init_device_logic();
        init_logging();
        init_signals();

	while (stop_execution == false) {
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
                        if (errno != EINTR) {
                                perror("recvfrom failed");
                        }
			// We continue anyway
		} else {
			process_incoming_packet(udp_socket, &src_addr, received_packet, ret,
                                        &options);
		}
	}

        fprintf(stderr, "Received signal %d, terminating\n", stop_execution_signal);

	free(received_packet);
	close(udp_socket);

	return 0;
}

