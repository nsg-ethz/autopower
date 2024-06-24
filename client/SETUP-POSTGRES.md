# Postgres database (Client)

Autopower uses a PostgreSQL based database on both the client and server to store measurement and application data. The current schema can be found in `workspace/server/database_schema.sql` or `workspace/client/client_db_schema.sql`.

If you want to deploy an autopower device, **do not** follow this guide, but rather run the deploy.sh script on a Pi instead! This document is for development and testing only!

## Setting up the Database on the client
The [Debian wiki](https://wiki.debian.org/PostgreSql) shows how to install PostgresSQL. After you have installed the server, create an user account for the client. Replace <password> with a secure password for the autopower_client user.
```bash
sudo -u postgres psql -d postgres -c "CREATE USER autopower_client WITH PASSWORD '<password>';"
```

Afterwards, create the `autopower_client` database:
```bash
sudo -u postgres psql -d postgres -c "CREATE DATABASE autopower_client;"
```

Now you can load the setup script for the database. If you use another database or user name, you may need to adapt the script client_db_schema.sql to fit to your needs.
```bash
sudo -u postgres psql -d autopower_client -a -f client_db_schema.sql
```
