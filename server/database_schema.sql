CREATE TABLE logmessages (
  log_id SERIAL PRIMARY KEY,
  client_uid VARCHAR(255) NOT NULL REFERENCES clients(client_uid),
  error_code INT NOT NULL,
  log_time TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
  log_message TEXT
);

CREATE TABLE clients (
  client_uid VARCHAR(255) PRIMARY KEY,
  last_seen TIMESTAMP WITH TIME ZONE DEFAULT NOW(), -- last time this client connected to the server, meaning we know it is alive
  ipv6_address INET,
  ipv4_address INET -- automatically populated
);

CREATE TABLE devices_under_test ( -- TODO: Rename to dut
  dut_id SERIAL PRIMARY KEY,
  dut_name VARCHAR(255),
  device_model VARCHAR(255),
  ipv6_address INET,
  ipv4_address INET,
  loc TEXT, -- location
  dut_owner TEXT
);

CREATE TABLE runs ( -- collection of measurements which correspond to one DUT. TODO: This currently allows multiple runs of the same dut at overlapping timestamps. May want to add a check()
  -- application should ensure that there is no run of the same dut in overlapping timeframes.
  run_id SERIAL PRIMARY KEY,
  dut_id INT REFERENCES devices_under_test(dut_id),
  start_run TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
  stop_run TIMESTAMP WITH TIME ZONE DEFAULT NULL,
  run_description TEXT, 
  CHECK(stop_run IS NULL OR start_run < stop_run)
);

CREATE TABLE client_runs ( -- link clients and runs
  client_run_id SERIAL PRIMARY KEY,
  client_uid VARCHAR(255) NOT NULL REFERENCES clients(client_uid),
  run_id INT REFERENCES runs(run_id) NOT NULL
);

CREATE TABLE measurements (
  server_measurement_id SERIAL PRIMARY KEY, -- numeric measurement id, different to the client side!
  shared_measurement_id VARCHAR(255) UNIQUE NOT NULL, -- measurement id shared with clients created by the client.
  run_id INT DEFAULT NULL REFERENCES runs(run_id), -- reference a run
  client_uid VARCHAR(255) NOT NULL REFERENCES clients(client_uid), -- the client this measurement came from. Should never be used as key/reference. Use client_runs relation instead
  pp_device VARCHAR(255) DEFAULT NULL, -- the device set by this measurement
  pp_sampling_interval INT DEFAULT NULL, -- the sampling interval of this measurement
  upload_intervalMin INT DEFAULT NULL -- the periodicity in minutes to upload the content from the clients to the server 
  -- has_finished_gracefully BOOLEAN DEFAULT FALSE -- saves if this measurement has been finished gracefully by the client and all data has been uploaded.
);

CREATE TABLE measurement_data (
  md_id SERIAL PRIMARY KEY,
  server_measurement_id INT NOT NULL REFERENCES measurements(server_measurement_id),
  measurement_value INT NOT NULL,
  measurement_timestamp TIMESTAMP WITH TIME ZONE NOT NULL
);

CREATE INDEX idx_msmt_and_timestamp ON measurement_data (server_measurement_id, measurement_timestamp);
GRANT ALL ON measurements, logmessages, runs, client_runs, measurement_data, clients, logmessages_log_id_seq, devices_under_test, measurement_data_md_id_seq, measurements_server_measurement_id_seq, devices_under_test_dut_id_seq, client_runs_client_run_id_seq, runs_run_id_seq TO autopower;
GRANT CREATE ON SCHEMA public TO autopower;
