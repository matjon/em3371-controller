
CREATE TABLE IF NOT EXISTS metrics_state (
   metrics_state_id     INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
-- Grafana requires the time column be in the UTC timezone
   time_utc             DATETIME NOT NULL,
   device_time          DATETIME
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

CREATE TABLE IF NOT EXISTS metrics_state_debug (
   metrics_state_id     INTEGER NOT NULL PRIMARY KEY,
-- TODO
   FOREIGN KEY(metrics_state_id) 
        REFERENCES metrics_state(metrics_state_id)
        ON DELETE cascade
);

CREATE TABLE IF NOT EXISTS sensor_state_debug(
   metrics_state_id     INTEGER NOT NULL,
   sensor_id            SMALLINT NOT NULL,
   temperature_raw      SMALLINT,
   temperature_min      NUMERIC(5,2),
   temperature_max      NUMERIC(5,2),
   humidity_min         SMALLINT,
   humidity_max         SMALLINT,
   PRIMARY KEY(metrics_state_id, metrics_state),
   FOREIGN KEY(metrics_state_id) 
        REFERENCES metrics_state(metrics_state_id)
        ON DELETE cascade
);
