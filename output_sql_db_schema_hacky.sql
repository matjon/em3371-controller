CREATE TABLE `weather_data` (
  `time` datetime NOT NULL,
  `atmospheric_pressure` int DEFAULT NULL,
  `station_temp` decimal(12,2) DEFAULT NULL,
  `station_humidity` int DEFAULT NULL,
  `station_dew_point` decimal(17,2) DEFAULT NULL,
  `sensor1_temp` decimal(12,2) DEFAULT NULL,
  `sensor1_humidity` int DEFAULT NULL,
  `sensor1_dew_point` decimal(17,2) DEFAULT NULL,
  PRIMARY KEY (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci 
