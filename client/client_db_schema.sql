CREATE TABLE IF NOT EXISTS measurements (
  internal_measurement_id SERIAL PRIMARY KEY,
  shared_measurement_id VARCHAR(255) NOT NULL UNIQUE, -- measurement ID for the server. May not be changed. Should be unique (distributed)
  client_uid VARCHAR(255) NOT NULL, -- the client_uid of the client saving this measurement. Allows us to upload datapoints from other clients in this DB (e.g. running two measurement binaries on one device)
  measurement_start_timestamp TIMESTAMP(4) WITH TIME ZONE NOT NULL DEFAULT (NOW())
);

CREATE TABLE IF NOT EXISTS measurement_data (
  md_id SERIAL PRIMARY KEY,
  internal_measurement_id INT REFERENCES measurements(internal_measurement_id),
  measurement_value INT NOT NULL,
  measurement_timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
  was_uploaded BOOLEAN DEFAULT FALSE
);

GRANT ALL ON measurements, measurement_data, measurement_data_md_id_seq, measurements_internal_measurement_id_seq TO autopower_client;
