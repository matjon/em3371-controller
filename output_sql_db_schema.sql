--  Copyright (C) 2020 Mateusz Jończyk
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License along
--  with this program; if not, write to the Free Software Foundation, Inc.,
--  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


CREATE TABLE IF NOT EXISTS metrics_state (
   metrics_state_id     INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
-- Grafana requires the time column be in the UTC timezone
   time_utc             DATETIME NOT NULL,
   device_time          DATETIME,
   INDEX metrics_state_time_index (time_utc)
);

CREATE TABLE IF NOT EXISTS sensor_reading (
   metrics_state_id     INTEGER NOT NULL,
   sensor_id            SMALLINT NOT NULL,
   atmospheric_pressure SMALLINT,
   temperature          NUMERIC(5,2),
   humidity             SMALLINT,
   dew_point            NUMERIC(5,2),
   PRIMARY KEY(metrics_state_id, sensor_id),
   FOREIGN KEY(metrics_state_id)
        REFERENCES metrics_state(metrics_state_id)
        ON DELETE cascade
);

CREATE TABLE IF NOT EXISTS sensor_reading_debug(
   metrics_state_id     INTEGER NOT NULL,
   sensor_id            SMALLINT NOT NULL,
   temperature_min      NUMERIC(5,2),
   temperature_max      NUMERIC(5,2),
   humidity_min         SMALLINT,
   humidity_max         SMALLINT,
   payload_0x31         SMALLINT,
   PRIMARY KEY(metrics_state_id, sensor_id),
   FOREIGN KEY(metrics_state_id)
        REFERENCES metrics_state(metrics_state_id)
        ON DELETE cascade
);

-- ALTER TABLE sensor_reading_debug ADD COLUMN payload_0x31 SMALLINT;

--------------------------------------------------------------------
-- The tables below are work in progress

CREATE TABLE IF NOT EXISTS device_packet_debug (
   metrics_state_id     INTEGER NOT NULL PRIMARY KEY,
-- TODO
   FOREIGN KEY(metrics_state_id)
        REFERENCES metrics_state(metrics_state_id)
        ON DELETE cascade
);

