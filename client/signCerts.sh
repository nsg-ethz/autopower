#!/bin/bash
# Romain Jacob

# do NOT run as root

source capassphrase.sh
source serverIpConfig.sh
DEVICENAME=$(hostname)

# ssh into the server (which works thanks to agent forwarding) 
tmux kill-session -t certs      # clean up if session already exists
tmux new-session -d -s certs    # create a tmux session
tmux send-keys -t certs '' C-m  # wait a bit
tmux send-keys -t certs '' C-m
tmux send-keys -t certs '' C-m
tmux send-keys -t certs "ssh autopower@${REMOTEHOST}" C-m

# clean existing read-only files
echo "cleaning existing files on the server..."
tmux send-keys -t certs 'cd /usr/autopower/zabbix/' C-m
tmux send-keys -t certs "rm -f zabbix_client_${DEVICENAME}.psk" C-m
tmux send-keys -t certs 'cd /usr/autopower/certs/' C-m
tmux send-keys -t certs "rm -f /usr/autopower/certs/client_${DEVICENAME}.csr" C-m
sleep 2 # wait a bit to give time to the tmux command to run


# copy the certificate to the server with scp
echo "copying the new files..."
sudo cp /etc/mmclient/client_${DEVICENAME}.csr .
scp client_${DEVICENAME}.csr autopower@${REMOTEHOST}:/usr/autopower/certs/client_${DEVICENAME}.csr
# copy the psk to wherever (probably the server as well, I should make a directory for that)
scp zabbix_psk.psk autopower@${REMOTEHOST}:/usr/autopower/zabbix/zabbix_client_${DEVICENAME}.psk

# sign the certificate on the server  
echo "signing the new certificate..."
SIGN_CMD="openssl x509 -req -in client_${DEVICENAME}.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out client_${DEVICENAME}.cer -days 365 -sha512 -passin pass:'${PASSPHRASE}'"
tmux send-keys -t certs "${SIGN_CMD}" C-m
sleep 2 # wait a bit to give time to the tmux command to run

# copy back client.cer and ca.cer (can be done via scp from the PI)
# > scp-ing directly would require to make the mmclient directory globally writable
echo "copying the signed certificate back on the client..."
scp autopower@${REMOTEHOST}:/usr/autopower/certs/client_${DEVICENAME}.cer ~/client.cer
scp autopower@${REMOTEHOST}:/usr/autopower/certs/ca.cer ~/ca.cer
sudo mv ~/*.cer /etc/mmclient/
sudo chown mmclient: /etc/mmclient/client.cer
sudo chown mmclient: /etc/mmclient/ca.cer

