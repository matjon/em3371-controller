Reverse engineered documentation of the weather station's protocol
==================================================================

I was reverse engineering the protocol of a Meteo SP73 weather station
manufactured in December 2018, but bought in late 2020. The weather station
contains a Lierda WiFi module.

I was using initial reveng work performed by Mr Łukasz Kalamłacki
( http://kalamlacki.eu/sp73.php ) as well as: observations, experiments,
monitoring communication to/from the EMAX server(s) and fuzzing.

This device can work in one of three modes of operation: UDP client, TCP client,
TCP server. I was working with the device in the UDP client mode, but from
initial assessment the TCP server mode seems similar.

To configure the station, open the device's IP address in a web browser (in a
not encrypted HTTP session to port 80). The default login credentials are
"admin" / "admin". Please note that the device's mode of operation and
its parameters get reset to default values after a power cycle and sometimes
even without one.

General packet structure
------------------------

| Offset | Length | Description |
| ------ | ------ | --------------------------- |
| 0x00   | 1      | Always '<'. A packet delimiter. Cf. the last byte of the packet. |
| 0x01   | 1      | Always 0x57, ASCII for 'W'. Maybe a product identifier, or simply "W" for "WiFi". |
| 0x02   | 1      | Always 0x01. Maybe a protocol revision number? |
| 0x03   | 4      | Last 4 bytes of the device's MAC address, most significant byte first. The byte at offset 0x03 is usually 0x69 as Lierda has the MAC address range 00:95:69:XX:XX:XX. |
| 0x07   | 1      | Function number. Determines the type of payload and/or the operation the receiver should perform. |
| 0x08   | 2      | Unknown, almost always each byte has value 0x00 or 0x01. Does not seem to matter much. |
| 0x0a   | 1      | `payload_size` - length of the packet payload |
| 0x0b   | 1      | Unknown, always zero in packets sent by the device. Sometimes other values seem to be ignored, sometimes they trigger an error. |
| 0x0c   | `payload_size` | Packet payload. |
| 0x0c + `payload_size`  | 1 | Control sum. A sum (modulo 256) of all bytes from offset 0x00 to (0x0c + `payload_size` - 1) inclusive. |
| 0x0d + `payload_size`  | 1 | Always '>'. Final delimiter. |

All packets sent by the station start with byte 0x3c and end with 0x3e.
These are probably delimiters, ASCII '<' and '>'.

When `payload_size` as specified in byte 0x07 does not match the real size of
the payload (= if the packet is longer then what byte 0x07 implies), the device
responds with an error packet with error code 0x02.

When the size of the payload is bigger then what the device expects (for
example, when sending a packet with function number 0x80 and payload size 0x10),
the device appears to silently ignore the extra length of the payload (in this
example, it uses only the first 0x08 bytes of it).

Known function numbers:
-----------------------

| Function number | Payload size  | Description |
| --------------- | ------------- | ----------- |
| 0x00            | 0x00          | A ping packet. The program is expected to respond with same packet. |
| 0x01            | 0x00          | Probably a ping packet also. |
| 0x01            | 0x39          | Weather station sensor data. |
| 0x02            | response: 0x03 | Unknown. The device responds with a packet with a function number 0x02 and a payload "0xff 0xff 0xff". Possibly some functionality that is not present on the specimen. |
| 0x20            | response: 0x08 | "Query device time". |
| 0x21 - 0x24     | response: 0x00 | Unknown. Device responds with a packet with same function number and payload of size 0x00. Perhaps some commands. |
| 0x80            | 0x08          | "Set device time." |
| 0x90            | ignored probably | "Send sensor data immediately". |
| 0xee            | 0x01          | Notification of an error condition. Only valid in one direction: when the weather station is the sender. |

Function numbers other then 0x00, 0x01 and 0x80 were discovered by fuzzing the
device by trying all possible function numbers with a long payload.

Function number 0x01, payload size 0x39 - weather station sensor data
---------------------------------------------------------------------

The weather station sends this packet automatically only if the handling
program (the program I am writing) sends back ping packets.

This packet is also sent immediately in response to a packet with function
number 0x90 "Set device time".

### Payload

| Offset | Length | Description |
| ------ | ------ | --------------------------- |
| 0x00   | 1      | Always 0x02. Protocol version? |
| 0x01   | 1      | Timezone (see below).          |
| 0x02   | 1      | Year after 2000 (e.g. 0x14 = year 2020) |
| 0x03   | 1      | Month of the year (0x0c = December) |
| 0x04   | 1      | Day of the month |
| 0x05   | 1      | Wall clock hour |
| 0x06   | 1      | Wall clock minutes |
| 0x07   | 1      | Seconds multiplied by 2, with a resolution of 0,5s. |
| 0x08   | 1      | Unknown, probably always 0x00 or 0x01. Looks like AM / PM flag. |
| 0x09   | 9      | Sensor data from sensors in the main weather station module (see below). |
| 0x12   | 9      | Sensor data from the remote sensor on channel 1. |
| 0x1b   | 9      | Sensor data from the remote sensor on channel 2. |
| 0x24   | 9      | Sensor data from the remote sensor on channel 3. |
| 0x2d   | 1      | A bit field of "battery low" alerts from remote sensors. (see below) |
| 0x2e   | 1      | Information on lost signal from sensors (see below). |
| 0x2f   | 2      | Atmospheric pressure in hPa, little-endian. Maybe 0xff 0xff when no data. |
| 0x31   | 1      | Unknown - has value 0x00, 0x10 or 0x20. Maybe trend of atmospheric pressure. |
| 0x32   | 7      | Always 0xff. Perhaps corresponding to some functionality not present in the specimen, or possibly for future expansion. |

Other values possibly present in the not decoded fields in the payload:
- low battery alerts of the weather station itself.

### Device time

After powering on the device, the date gets set to 2016-01-01.
Sometimes the device sends incorrect time data such as "2020-12-15 10:60:00"
instead of the expected "2020-12-15 11:00:00", it is necessary to account for
this. The time is in local timezone, exactly just as is shown on the device's
screen.

See discussion of timezone in description of "function number 0x80 - set weather
station clock".

### Sensor data format

When the respective sensor is not present, all 9 bytes corresponding to this
sensor have value 0xff. It may be possible that only some bytes have this value
(for example, when humidity is known but temperature is unknown), but I have not
seen such packets.

| Offset | Length | Description |
| ------ | ------ | --------------------------- |
| 0x00   | 3      | Current humidity and temperature. |
| 0x03   | 3      | Maximum humidity and temperature (since the weather station was powered on). |
| 0x06   | 3      | Minimum humidity and temperature. |

Each of these sets of the three bytes has the following format:

| Offset | Length | Description |
| ------ | ------ | --------------------------- |
| 0x00   | 2      | `raw_temperature`, little-endian. See below for details. |
| 0x02   | 1      | Humidity, in % . |

The formula to calculate temperature in °F from `raw_temperature` sent by the
device is:

        temperature_fahrenheit = raw_temperature / 10 - 90;

When the device is configured to display temperature in °F, the temperature
obtained this way matches exactly the one displayed on the device's screen.

Minimum and maximum values of humidity and temperature are taken since the
weather station was powered on. They are shown on the device's screen after
pressing the MEM button. The values can be reset by pressing the MEM button for
several seconds. They also are reset after the remote sensor is being paired
again with the device.
Minimum temperature and humidity can have been taken at
different times, therefore calculating metrics from them (such as dew point)
would be incorrect - likewise for maximum values.

### Byte 0x2d - battery low alerts

Byte 0x2d of the payload is a bit field:

| Bit number | Description |
| ---------- | --------------------------- |
| 0          | Unknown, always 0. |
| 1          | Battery low alert from sensor on channel 1. |
| 2          | Battery low alert from sensor on channel 2. |
| 3          | Battery low alert from sensor on channel 3. |
| 4-7        | Unknown, always 0. |

### Byte 0x2e - lost sensors

Byte 0x2e of the payload is a bit field:

| Bit number | Description |
| ---------- | --------------------------- |
| 0          | Lost signal from sensor on channel 1. |
| 1          | Lost signal from sensor on channel 2. |
| 2          | Lost signal from sensor on channel 3. |
| 3-7        | Unknown, always 0. |

After powering off a remote sensor:

- after around 5 minutes the small "radio signal" symbol on weather station
  display disappears. Unfortunately, it is AFAIK not possible to get this
  information through network API. Old metrics from this sensor are displayed
  unchanged on the display and are sent in packets.
- 1 hour after powering off: the corresponding bit of byte 0x2e
  gets set. The display starts alternating between "--.-" and last known metrics
  from the respective sensor.
- 2h after powering off: in weather station sensor data (function number 0x01)
  packets corresponding bytes get all set to 0xff. The corresponding bit in byte
  0x2e of payload is still set.

Therefore, after receiving a packet with some bits in byte 0x2e set, the
application may wish to flag all data from the appropriate sensor(s) from last 
1 hour as unreliable.

TODO: check if after the sensor becomes available again (before the 1 hour mark)
the min/max metrics are reset.

Function number 0x80 - set weather station clock
------------------------------------------------

To set the weather station clock, send to it a packet with function code 0x80
and payload of 8 bytes.

Bytes 0x00-0x06 of this payload should contain local time, they
correspond to bytes 0x01-0x07 of weather station sensor data payload (see
"Function number 0x01, payload size 0x39" and also discussion there). In
particular, seconds (in byte 0x06) should also be multiplied by 2.

Byte 0x07 of the payload: the server at SMARTSERVER.EMAXTIME.CN sets this byte
to 0x00, other values of this byte seem to do nothing interesting.

The device should respond with the same packet back (but clears byte 0x08 of
whole packet (not of the payload), if it was set to 0x01).

Setting the weather station clock using function 0x80 increases the period
between consecutive sensor data reports (how often the device sends 0x01 packets
containing sensor data) - from 12.5s to around 107s. Setting the time manually
using buttons on the back of the device does not cause such side-effects.
From my perspective, this is unfortunate. I have tried a large number of various
tricks to change this behavior and this was the primary motivation of my fuzzing
the weather station. I have found no other way around this limitation other
than querying the station manually using packets with function number 0x90,
though.

### Timezone

Byte 0x00 of payload of function 0x80 contains the desired device timezone.

It is possible to set the timezone manually by pressing the SET button on
the back of the device for several seconds, then pressing it once. Then use the
UP and DOWN buttons to change the value.
According to the manual I got with the weather station, timezone "00"
corresponds to the Central European Time (GMT+1), but experiments seem to
contradict this. It looks like timezone "00" corresponds to GMT.

This parameter seems mostly unused, though. It does not survive power cycles of
the device (like probably all other parameters except for those of the Lierda
WiFi module). The device always sends time in local timezone.

Byte values of the timezone: GMT is represented by timezone 0x0c (12 in
decimal). This byte AFAIK can have values from 0x00 (which corresponds to
GMT-12) to 0x18 (which corresponds to GMT+12).

The SMARTSERVER.EMAXTIME.CN server has set the device to timezone 0x14 (using
packet with function 0x80), which corresponds to GMT+8, which is the timezone
used in China. The local device time has been set accordingly with the GMT+8
offset. This can be annoying for some users. Possibly the server sets the
correct timezone after using the LivingSense app or remembering the timezone set
by the user.

Function number 0x90 - query device sensor state
------------------------------------------------

The payload of a packet with a function number 0x90 seems to be ignored.
The device responds with a packet with a function number 0x01 containing sensor
data.
