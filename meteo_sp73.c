
#include "meteo_sp73.h"
#include "main.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>


/*
 * Using packed structs with casting may be unsafe on some architectures:
 * 	https://stackoverflow.com/questions/8568432/is-gccs-attribute-packed-pragma-pack-unsafe
 * Therefore I'm parsing the packet by hand.
 */

/*
 * Accesses three bytes from raw_data.
 * Returns true if at least some data is present.
 */
bool decode_single_measurement(struct device_single_measurement *measurement,
		const unsigned char *raw_data)
{
	if (raw_data[0] == 0xff && raw_data[1] == 0xff && raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		return false;
	}

	if (raw_data[0] == 0xff && raw_data[1] == 0xff) {
		measurement->temperature = DEVICE_INCORRECT_TEMPERATURE;
		fprintf(stderr, "Weird: measurement contains humidity, but not temperature.\n");
	} else {
		// Trying to be endianness-agnostic
		uint16_t raw_temperature = raw_data[0] + ((int)raw_data[1]) * 256;
		measurement->raw_temperature = raw_temperature;

		// The reverse engineering documentation contains the following formula for
		// calculating the real value of the temperature:
		// 	 f(x) = 0,0553127*x - 67,494980
		// The origin of both numbers is, however, not described. They may
		// have been obtained from lineal regression or something similar.
		//
		// It may be possible that the device sends the value in degrees Fahrenheit
		// multiplied by 10 with some offset.
		//
		// TODO: obtain more accurate formula
		measurement->temperature = 0.0553127 * (float)raw_temperature - 67.494980;
	}

	if (raw_data[2] == 0xff) {
		measurement->humidity = DEVICE_INCORRECT_HUMIDITY;
		fprintf(stderr, "Weird: measurement contains temperature but not humidity.\n");
	} else {
		measurement->humidity = raw_data[2];
	}

	return true;
}

bool decode_single_sensor_data(struct device_single_sensor_data *out,
		const unsigned char *raw_data)
{
	bool have_current_data = decode_single_measurement(&(out->current), raw_data);
	bool have_historical1_data = decode_single_measurement(&(out->historical1), raw_data + 3);
	bool have_historical2_data = decode_single_measurement(&(out->historical2), raw_data + 6);

	out->any_data_present = have_current_data || have_historical1_data || have_historical2_data;

	return out->any_data_present;
}

void decode_sensor_state(struct device_sensor_state *state, const unsigned char *received_packet,
		const size_t received_packet_size)
{
	if (received_packet_size <= 61) {
		fprintf(stderr, "Packet is too short!\n");
		return;
	}

	decode_single_sensor_data(&(state->station_sensor), received_packet + 22);
	for (int i = 0; i < 3; i++) {
		decode_single_sensor_data(&(state->remote_sensors[i]), received_packet + 31 + i*9);
	}

	if (received_packet[59] == 0xff && received_packet[60] == 0xff) {
		state->atmospheric_pressure = DEVICE_INCORRECT_PRESSURE;
	} else {
		state->atmospheric_pressure = received_packet[59] + received_packet[60]*256;
	}
}


void display_single_measurement_json(FILE *stream, const struct device_single_measurement *state)
{
	fprintf(stream, "{");

	if (!DEVICE_IS_INCORRECT_TEMPERATURE(state->temperature)) {
		fprintf(stream, " \"temperature\": %.2f, \"raw_temperature\": %d",
			(double) state->temperature, (int) state->raw_temperature);

                if (state->humidity != DEVICE_INCORRECT_HUMIDITY) {
                        fputs(",", stream);
                }
	}

	if (state->humidity != DEVICE_INCORRECT_HUMIDITY) {
		fprintf(stream, " \"humidity\": %d", (int) state->humidity);
	}

	fprintf(stream, " }");
}

void display_single_sensor_json(FILE *stream, const struct device_single_sensor_data *state)
{
	fprintf(stream, "{\n");

	fprintf(stream, "    \"current\": ");
	display_single_measurement_json(stream, &(state->current));

	fprintf(stream, ",\n    \"historical1\": ");
	display_single_measurement_json(stream, &(state->historical1));

	fprintf(stream, ",\n    \"historical2\": ");
	display_single_measurement_json(stream, &(state->historical2));

	fprintf(stream, "\n  }");
}

void display_sensor_state_json(FILE *stream, const struct device_sensor_state *state)
{
	fputs("{ ", stream);
	if (state->atmospheric_pressure != DEVICE_INCORRECT_PRESSURE) {
		fprintf(stream, "\"atmospheric_pressure\": %u,\n", state->atmospheric_pressure);
	}
	fprintf(stream, "  \"station_sensor\": ");

	display_single_sensor_json(stream, &(state->station_sensor));

	for (int i = 0; i <= 2; i++) {
                if (state->remote_sensors[i].any_data_present) {
                        // The sensors are not interchangeable.
                        // Therefore I'm not using an array, but a key-value table.
                        fprintf(stream, ",\n  \"sensor%d\": ", i+1);
                        display_single_sensor_json(stream, &(state->remote_sensors[i]));
                }
	}

	fprintf(stream, "\n}\n");
}

// Main program logic
void process_incoming_packet(int udp_socket, const struct sockaddr_in *packet_source,
		const unsigned char *received_packet, const size_t received_packet_size)
{
	dump_incoming_packet(stderr, packet_source, received_packet, received_packet_size);

	if (received_packet_size < 20) {
#ifdef CONFIG_REPLY_TO_PING_PACKETS
	// See description in config.h
	// TODO: check the contents of the packet, not just its size
		reply_to_ping_packet(udp_socket, packet_source,
				received_packet, received_packet_size);
#endif
	} else if (received_packet_size >= 65) {

		struct device_sensor_state *sensor_state;
		sensor_state = malloc(sizeof(struct device_sensor_state));
                if (sensor_state == NULL){
		        fprintf(stderr, "process_incoming_packet: Cannot allocate memory!\n");
                        return;
                }

		decode_sensor_state(sensor_state, received_packet, received_packet_size);
		display_sensor_state_json(stderr, sensor_state);

		free(sensor_state);
	}
}
