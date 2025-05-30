RELEASE_DOWNLOAD_VERSION="3.0.3" # version to download from GitHub

# Edit this file to use a different IP and host
REMOTEHOST="ee-tik-nsgvm057.ethz.ch"
REMOTEIP="129.132.31.132"
REMOTEIP6="2001:67c:10ec:2a40::31"
# Zabbix config
REMOTEZABBIXHOST="ee-tik-nsgvm057.ethz.ch"
REMOTEZABBIXIP="129.132.31.132"
REMOTEZABBIXIP6="2001:67c:10ec:2a40::31"
USE_JUMPHOST="no" # yes/no. yes will use autopowerconnect@${EXTERNALJUMPHOST}
# ^ if I jumphost, then the no key checking does not get applied to the correct SSH session. 
# Removing the jumphost is the simplest, given that I most often flash devices within the ETH network.
NOKEYCHECK="-o StrictHostKeyChecking=no"
