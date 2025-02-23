# Deploying an Autopower device

This document serves as writeup for deployment of an autopower device in the data center. Before an autopower device can be deployed, it should have been registered and tested at least once during flashing (initial setup). We assume this has been the case.

## Hardware setup
1. Plug in the power meter in a USB port of the Raspberry Pi
2. Set up internet access for this autopower device. **Note:** Dual Stack (IPv4 and IPv6) is preferred. The devices should also work behind NAT, or IPv4 only networks, however it is preferrable to have both protocols enabled. By default, the devices get their adresses via DHCP. Custom/other configuration needs to be done on the operating system level during initial setup.
3. Connect the Ethernet cable to the Pi
4. Connect the Power Meter to the device under test
5. Connect the Raspberry Pi to power

After some time, the Raspberry Pi should register and show that it is measuring in the Management UI.

* If the red PWR LED is blinking three times in a row periodically for an extended period of time, there is a connectivity issue to the server. Please check the network connectivity of the Autopower device. Ping the IP of the Autopower device. If it responds, restart mmclient on the Autopower device via ssh (`systemctl restart mmclient`) if you have the rights. Before power cycling the device, ensure if any firewall or similar blocks outgoing messages from the Pi to the ETH/SWITCH network to rule out network issues.
* If the ACT LED is blinking 4 times in a row right after starting the Autopower device, the measurement could not start successfully. Check the USB connection to the power meter and restart the Pi if necessary.

In a working environment, the power meter should blink periodically and the Management UI should show recent data uploads. By default, the recently uploaded data should not be older than 5 to 6 minutes. 

## Ports to open

Strictly speaking, there is no need to forward or open any ports from an external network to the Raspberry Pies. For convenience and easier maintenance reasons, it is beneficial to...

* forward/open port `21092` (TCP) for fall back SSH access (recommended).
* forward/open port `10050` (TCP) for Zabbix monitoring (only used as fallback for monitoring, not necessary, enable for convinience)
