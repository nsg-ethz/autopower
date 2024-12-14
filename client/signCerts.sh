#!/bin/bash
# Romain Jacob

# do NOT run as root

source capassphrase.sh
source serverIpConfig.sh
source serverAdminConfig.sh
DEVICENAME=$(hostname)

# ssh into the server (which works thanks to agent forwarding) 

# clean existing read-only files
echo "cleaning existing files on the server..."
ssh ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST} -t "rm -f /usr/autopower/zabbix/zabbix_client_${DEVICENAME}.psk"
ssh ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST} -t "rm -f /usr/autopower/certs/client_${DEVICENAME}.csr"
ssh ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST} -t "rm -f /usr/autopower/certs/client_${DEVICENAME}.cer"


# copy certificates to the server with scp
echo "copying the new files..."
sudo cp /etc/mmclient/client_${DEVICENAME}.csr .
scp ${NOKEYCHECK} ${JUMPHOST} client_${DEVICENAME}.csr autopower@${REMOTEHOST}:/usr/autopower/certs/client_${DEVICENAME}.csr

# sign the certificate on the server  
echo "signing the new certificate..."

SIGN_CMD="openssl x509 -req -in /usr/autopower/certs/client_${DEVICENAME}.csr -CA /usr/autopower/certs/ca.cer -CAkey /usr/autopower/certs/ca.key -CAcreateserial -out /usr/autopower/certs/client_${DEVICENAME}.cer -days 365 -sha512 -passin pass:'${PASSPHRASE}'"
ssh ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST} -t "${SIGN_CMD}"


# copy back client.cer and ca.cer (can be done via scp from the PI)
# > scp-ing directly would require to make the mmclient directory globally writable
echo "copying the signed certificate back on the client..."
scp ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST}:/usr/autopower/certs/client_${DEVICENAME}.cer ~/client.cer
scp ${NOKEYCHECK} ${JUMPHOST} autopower@${REMOTEHOST}:/usr/autopower/certs/ca.cer ~/ca.cer
sudo mv ~/*.cer /etc/mmclient/
sudo chown mmclient: /etc/mmclient/client.cer
sudo chown mmclient: /etc/mmclient/ca.cer

# copy and chown ssh key for reverse ssh
sudo cp /home/reversessh/.ssh/id_ed25519.pub reversessh_ed25519.pub
scp ${NOKEYCHECK} reversessh_ed25519.pub autopowerconnect@${EXTERNALJUMPHOST}:/tmp/sshcert_${DEVICENAME}.pub

# Add external sshkey to autopowerconnect user on the server
SAVE_CMD="cat /tmp/sshcert_${DEVICENAME}.pub >> /local/home/autopowerconnect/.ssh/authorized_keys"
ssh -t ${NOKEYCHECK} autopowerconnect@${EXTERNALJUMPHOST} "${SAVE_CMD}"

# let reversessh user connect (trial)
sudo -u reversessh mkdir /home/reversessh/.ssh/
sudo touch /home/reversessh/.ssh/known_hosts
sudo chown reversessh:reversessh /home/reversessh/.ssh/known_hosts
sudo -u reversessh sh -c "ssh-keyscan -t ed25519 ${EXTERNALJUMPHOST} >> /home/reversessh/.ssh/known_hosts"
sudo -u reversessh -s ssh autopowerconnect@${EXTERNALJUMPHOST} -t "echo 'Connection to autopowerconnect works'"