
#define CONFIG_BIND_ADDRESS "0.0.0.0"
#define CONFIG_BIND_PORT 1234

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
#define CONFIG_REPLY_TO_PING_PACKETS
