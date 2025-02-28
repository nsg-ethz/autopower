# Monitoring the Autopower devices via Zabbix

## Application monitoring
Zabbix periodically (every hour) calls `http://localhost:5000/device/status/all` which gives back a JSON-Response of the current state of the whole deployment gathered by the Management UID. Changes here are only logged, not interpreted. With more involved setup in Zabbix, this could be used to detect specific bad conditions in a very detailled way. For now this is just a convinience metric.

## Database monitoring

Zabbix also monitors the PostgreSQL database via ODBC on the server to detect > 5 second data loss, clients which no longer measure, new log messages uploaded by the clients and if the latest measurement timestamp changed. This gives metrics to see if there are errors. If the output of the following SQL queries changes/doesn't change in case of "Latest measurement timestamp", Zabbix sends a message to Slack to inform the administrator of potential errors.

### > 5 second measurement data loss
```sql
SELECT CONCAT('Jump of ', time_difference, ' in measurement at ', measurements.client_uid, ' from ', previous_ts, ' until ',  measurement_timestamp) FROM (SELECT LAG(measurement_timestamp) OVER (PARTITION BY server_measurement_id ORDER BY measurement_timestamp) AS previous_ts, measurement_timestamp - LAG(measurement_timestamp) OVER (PARTITION BY server_measurement_id ORDER BY measurement_timestamp) AS time_difference, measurement_timestamp, server_measurement_id FROM measurement_data GROUP BY server_measurement_id, measurement_timestamp) AS differences, measurements WHERE time_difference >= INTERVAL '5 second' AND differences.server_measurement_id IN (SELECT server_measurement_id FROM measurements) AND measurements.server_measurement_id = differences.server_measurement_id LIMIT 5;
```

Gets jumps in measurement samples. This allows to detect data got lost. Can e.g. happen if there was a hardware failure. If no data is found on the clients, the data is lost. Missing data can be re-uploaded by setting the `was_uploaded` flag on the respective client to `false` and restarting a measurement. 

### Clients no measurments since one hour

```sql
SELECT measurements.client_uid, MAX(measurement_timestamp) > NOW() - INTERVAL '1 hour' AS has_recent_measurement FROM measurement_data JOIN measurements USING (server_measurement_id) GROUP BY measurements.client_uid;
```

Gets the measurement status of all clients. If a client hasn't been uploading data for > 1 hour, `has_recent_measurement` is set to 0. Allows to detect if a client stopped uploading data - for any reason. Administrators can restart `mmclient` on the respective client.

### Latest Log message

```sql
SELECT log_message, log_time, client_uid FROM logmessages ORDER BY log_time DESC LIMIT 1;
```

Gets the latest log message from the `logmessages` table. If a client received an error, it will send a log message which Zabbix detects.

### Latest measurement timestamp
```sql
SELECT EXTRACT(epoch FROM MAX(measurement_timestamp))::integer FROM measurement_data;
```

Gets the latest measurment timestamp from the measurement_data table. This can be used to get notified in case no client uploads any data and the database stays unchanged for a log period. This could point at a crash of `mmserver`.