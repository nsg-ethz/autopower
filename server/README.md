# Autopower Server

The autopower server consists of multiple files:
* `server.py`: The communication to the autopower clients. This script can a shell to send commands to each registered autopower client if ran with the `--interactive` parameter.
* `plotter/`: A dash application to visualize the data from a web UI.
* `management/`: A flask application to control the clients via web UI.
* `database_schema.sql`: The database schema for the PostgreSQL database running on the server storing the measurements, settings, logs,...

## Installation of server
### Python dependencies
To install the server, a recent version of python3 is required. Install python3 and all dependencies as follows:
`sudo apt install python3 python3-pip python3-grpcio python3-psycopg2 virtualenv`.

### PostgreSQL

Autopower uses a PostgresSQL based database on both the client and server to store measurement and application data.

The [Debian wiki](https://wiki.debian.org/PostgreSql) shows how to install PostgresSQL. On ETH infrastructure, you may want to ensure that PostgreSQL uses the correct, local user. The easiest way is to:
1. Login as root
2. Stop sssd: `systemctl stop sssd`. !!!DO NOT LOG OUT BEFORE SSSD HAS BEEN STARTED AGAIN!!!
3. Install PostgreSQL (e.g. with `sudo apt install postgresql`)
4. Restart sssd: `systemctl start sssd`

After you have installed the server, create an user account for the server. Replace <password> with a secure password for the autopower user. 
```bash
sudo -u postgres psql -d postgres -c "CREATE USER autopower WITH PASSWORD '<password>';"
```

Afterwards, create the `autopower` database:
```bash
sudo -u postgres psql -d postgres -c "CREATE DATABASE autopower;"
```

Now you can load the setup script for the database. If you use another database or user name, you may need to adapt the script database_schema.sql to fit to your needs. Be sure that the postgres user has read access to database_schema.sql and you are in the `server` directory of this repo. 
```bash
sudo -u postgres psql -d autopower -a -f database_schema.sql
```

### Encryption setup

Generate the CA and server keys by running ../certs/create_certs.sh. You may want to modify the parameters in the script to fit to your setting (e.g changing the CN, Organization etc.).
Make the server.key file readable to the user running server.py.

### Setting up the server

To deploy the server:
* Log in as root
* If not already done, create a user called `autopower`: `adduser autopower`
* Create a directory to save all the content from the server/ directory: `mkdir /usr/autopower`
* Change to the directory you just created: `cd /usr/autopower`
* Create a virtualenv: `virtualenv venv`
* Enter the virtual environment: `source venv/bin/activate`
* Install the requirements: `pip3 install -r requirements.txt`
* Create a symlink to the config directory: `ln -s /usr/autopower/config/ /etc/autopower`
* Set permissions to autopower readonly: `chown root:autopower /etc/autopower && chmod u=rwx,g=rx,o= /etc/autopower`
* Edit the `secrets.json` by setting the correct paths to certificates and the database connection (`nano /etc/autopower/secrets.json`). Place all the certificates in a place and set permissions such that the autopower user can read the certificates `chown root:autopower /path/to/srv.key && chmod u=r,g=r,o= /path/to/srv.key`. Repeat that for all the certificates specified in the config:
```
{
    "postgres": {
      "host": "localhost",
      "database": "autopower",
      "user": "autopower",
      "password": "<securePasswordSetForAutopowerUser>"
    },
    "ssl": {
      "privKeyPath": "/path/to/server/private/key/srv.key",
      "pubKeyPath": "/path/to/server/public/key/srv.cer",
      "pubKeyCA": "/path/to/ca/certificate/for/clients/ca.cer"
    }
}
```

* Adapt the `server_config.json` file and set on which port and name to listen on (`nano /etc/autopower/server_config.json`):
```
{
    "listenOn": "example.com:25181"
}
```
* Set permissions to only make the secrets file readable by the autopower user: `chown root:autopower /etc/autopower/secrets.json`, `chmod u=r,g=r,o= /etc/autopower/secrets.json`
* Copy the mmserver.service systemd service definition file to setup the service: `cp /usr/autopower/mmserver.service /etc/systemd/system`
* Enable autostart: `systemctl enable mmserver.service`
* Start the server: `systemctl start mmserver.service`

## Connecting to server via CLI:
* The `cli.py` script enables you to connect to a server via a basic shell.
* Go into the autopower folder: `cd /usr/autopower`
* To use `cli.py`, configure you may need to change the hostname and port the server runs: `cp config/cli_config.json.example config/cli_config.json && nano config/cli_config.json`
* Create and sign keys for the cli client as described in the /certs/ folder
* Now set up the paths to these keys in the `cli_secrets.json file`: `cp config/cli_secrets.json.example config/cli_secrets.json && nano config/cli_secrets.json`. The format closely follows the `ssl` section of the `secrets.json` file of the server.
* Set permissions to only make the cli_secrets file readable by the autopower user: `chown root:autopower /etc/autopower/cli_secrets.json`, `chmod u=r,g=r,o= /etc/autopower/cli_secrets.json`
* Enter the virtual environment `source venv/bin/activate` and connect to the server via `python3 cli.py`. You can now issue commands to the server.

## Zabbix monitoring

If you set up [ODBC monitoring via Zabbix](https://www.zabbix.com/documentation/6.4/en/manual/config/items/itemtypes/odbc_checks/) of the server database, create a new user e.g. called `zbxapmonitor` and grant SELECT privileges on all tables e.g. via:
```sql
GRANT SELECT ON clients,logmessages,devices_under_test,runs,client_runs,measurements,measurement_data TO zbxapmonitor;
```


## Setting up reverse SSH tunelling

1. On the server, add a user to which all clients can connect via SSH `sudo adduser autopowerconnect --shell=/bin/false` and set a random password.
2. Login as this user: `sudo -u autopowerconnect -s` or login as root and then change to autopowerconnect: `sudo --login` as root: `sudo -u autopowerconnect -s`
3. Create a ssh directory: `cd && mkdir .ssh`
4. Copy ssh key to authorized_keys: `echo "<key>" >> ~/.ssh/authorized_keys`
5. On the client, run `ssh -fN -R <someportforserver>:localhost:21092 autopowerconnect@<jumphost>`

**Checkout** [jfrog.com](https://jfrog.com/connect/post/reverse-ssh-tunneling-from-start-to-end/) for more information.
