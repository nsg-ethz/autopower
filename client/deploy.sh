#!/bin/bash

# Deploy an autopower device should only be run on the Raspberry Pi

source serverIpConfig.sh
source serverAdminConfig.sh

# > we use the hostname configured on OS install
#read -p "Enter name of device to deploy: " DEVICENAME
DEVICENAME=$(hostname)
# Split out autopower number
AUTOPOWERNUMBER="${DEVICENAME/autopower/''}"
if ! [[ "${AUTOPOWERNUMBER}" =~ ^[0-9]+$ ]]; then
  echo "Got invalid hostname. The hostname must be of the form: autopower<number>. E.g: autopower1"
  exit 1
fi

# install needed services (including adding zabbix)
pushd /tmp
wget https://repo.zabbix.com/zabbix/6.0/raspbian/pool/main/z/zabbix-release/zabbix-release_6.0-5+debian12_all.deb
dpkg -i zabbix-release_6.0-5+debian12_all.deb
rm zabbix-release_6.0-5+debian12_all.deb
popd
apt update
apt upgrade -y
apt install autossh libjsoncpp-dev libpqxx-dev fail2ban ufw postgresql unattended-upgrades zabbix-agent2 zabbix-agent2-plugin-postgresql tmux -y

# install mmclient and pinpoint
cp bin/mmclient /usr/bin/mmclient
chmod +x /usr/bin/mmclient
cp bin/pinpoint /usr/bin/pinpoint
chmod +x /usr/bin/pinpoint
# set hostname
# hostnamectl set-hostname "${DEVICENAME}"

# Add hostname to /etc/hosts
echo "::1 ${DEVICENAME}" >> /etc/hosts
echo "127.0.0.3  ${DEVICENAME}" >> /etc/hosts

# add server to /etc/hosts
if [ -n "${REMOTEIP6}" ]; then
  echo "${REMOTEIP6}  ${REMOTEHOST}" >> /etc/hosts
fi

echo "${REMOTEIP}  ${REMOTEHOST}" >> /etc/hosts

# set up postgres
# password as per https://stackoverflow.com/questions/44376846/creating-a-password-in-bash
# This will not give special characters. May still be worth escaping in future
PGPASSWORD=$(cat /dev/urandom | tr -dc A-Za-z0-9~_- | head -c 60 && echo)
sudo -u postgres psql -d postgres -c "CREATE USER autopower_client WITH PASSWORD '${PGPASSWORD}';"
sudo -u postgres psql -d postgres -c "CREATE DATABASE autopower_client;"
sudo -u postgres psql -d autopower_client -a -f client_db_schema.sql

# Create group to allow access to on board leds
addgroup leds
# create mmclient user and set up files
adduser --system mmclient
# allow access to power meter
adduser mmclient dialout
# Allow access to on board LEDs
adduser mmclient leds
# Setup reverse SSH user
adduser --disabled-password --gecos "" reversessh
# Create key for reversessh user
sudo -u reversessh mkdir /home/reversessh/.ssh/
sudo -u reversessh ssh-keygen -f /home/reversessh/.ssh/id_ed25519 -t ed25519 -N ""

# Copy config files
mkdir /etc/mmclient
cp config/secrets.json.example /etc/mmclient/secrets.json
chmod u=r,g=,o= /etc/mmclient/secrets.json

# replace magic string ßß§$$$rplacePw$$$§ßß with actual password
sed -i 's/ßß§$$$rplacePw$$$§ßß/'"${PGPASSWORD}"'/' /etc/mmclient/secrets.json
echo "Creating client certificates..."
./deploy/create_client_cert.sh "${REMOTEHOST}"
mv client.key /etc/mmclient/client.key
chmod u=r,g=,o= /etc/mmclient/secrets.json
mv client.csr /etc/mmclient/client_"${DEVICENAME}".csr

# setup config files
cp config/client_config.json.example /etc/mmclient/client_config.json
sed -i 's/ßß§$$$rplceremoteHost$$$§ßß/'"${REMOTEHOST}"'/' /etc/mmclient/client_config.json
sed -i 's/ßß§$$$rplceClientUid$$$§ßß/'"${DEVICENAME}"'/' /etc/mmclient/client_config.json
chmod u=rx,g=,o= /etc/mmclient
chown -R mmclient: /etc/mmclient
# copy autostart systemd service for powermeasurement
cp deploy/mmclient.service /etc/systemd/system/
systemctl enable mmclient
# copy reversessh service
cp deploy/reversessh.service /etc/systemd/system/
# Replace magic strings with device dependent config values
sed -i 's/ßß§$$$rplceremoteHost$$$§ßß/'"${EXTERNALJUMPHOST}"'/' /etc/systemd/system/reversessh.service
sed -i 's/ßß§$$$rplceremoteRevSSHPort$$§ßß/'"28${AUTOPOWERNUMBER}"'/' /etc/systemd/system/reversessh.service

systemctl enable reversessh
# copy udev rule to allow leds group to access leds
cp deploy/99-leds.rules /etc/udev/rules.d/99-leds.rules

# set timezone
timedatectl set-timezone "Europe/Zurich"
# copy ssh key
echo "Copying ssh key"
mkdir /home/ethditet/.ssh
cat deploy/ssh_key.pub >> /home/ethditet/.ssh/authorized_keys
chown ethditet:ethditet /home/ethditet/.ssh/authorized_keys

cp deploy/sshd_autopower.conf /etc/ssh/sshd_config.d/sshd_autopower.conf

# setup zabbix
cp deploy/zabbix_agent_autopower.conf.example /etc/zabbix/zabbix_agent2.d/zabbix_agent_autopower.conf
sed -i 's/ßß§$$$rplceremoteHost$$$§ßß/'"${REMOTEHOST}"'/' /etc/zabbix/zabbix_agent2.d/zabbix_agent_autopower.conf
sed -i 's/ßß§$$$rplceClientUid$$$§ßß/'"${DEVICENAME}"'/' /etc/zabbix/zabbix_agent2.d/zabbix_agent_autopower.conf
# generate random psk for zabbix (according to official docs of zabbix: https://www.zabbix.com/documentation/current/en/manual/encryption/using_pre_shared_keys)
#openssl rand -hex 32 > zabbix_psk.psk
#cp zabbix_psk.psk /etc/zabbix/psk.psk
# Use autoregistration PSK
echo "${ZABBIXPSK}" > /etc/zabbix/psk.psk
chmod u=r,g=r,o= /etc/zabbix/psk.psk
chown root:zabbix /etc/zabbix/psk.psk

# Enable zabbix
systemctl restart zabbix-agent2
systemctl enable zabbix-agent2

# enable firewall and only allow ssh on port 21092 and zabbix agent to ${REMOTEIP}/${REMOTEIP6} - this assumes that zabbix is installed on ${REMOTEIP}/${REMOTEIP6}
ufw default deny incoming
ufw default allow outgoing
ufw allow 21092/tcp
ufw allow from "${REMOTEIP}" to any port 10050 proto tcp
ufw allow from "${REMOTEIP6}" to any port 10050 proto tcp
ufw logging off
ufw --force enable

echo "Please copy /etc/mmclient/client_${DEVICENAME}.csr to the server and sign the certificate request. Afterwards setup zabbix monitoring with the psk in ./zabbix_psk.psk"

