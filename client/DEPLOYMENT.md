# Deploying an Autopower device

This document serves as writeup for deployment of an autopower device in the data center. Before an autopower device can be deployed, it should have been registered and tested at least once during flashing (initial setup). We assume this has been the case.

## Hardware setup
1. Plug in the power meter in a USB port of the Raspberry Pi
2. Set up internet access for this autopower device. **Note:** Dual Stack (IPv4 and IPv6) is preferred. The devices should also work behind NAT, or IPv4 only networks, however it is preferrable to have both protocols enabled. By default, the devices get their adresses via DHCP. Custom/other configuration needs to be done on the operating system level during initial setup.
3. Connect the Ethernet cable to the Pi
4. Connect the Power Meter to the device under test
5. Connect the Raspberry Pi to power

![PowerMeterSetup](https://github.com/user-attachments/assets/16b141c6-653e-41a7-8a34-5f6b43fcdc2d)

Please check if any of the LEDs on the Raspberry Pi show one of the following behaviours. If this is the case, there is a problem:

* If the red [PWR LED is blinking three times in a row (video)](https://github.com/user-attachments/assets/58c559cf-ebc2-44a6-bb93-a173dd1b309d) periodically for an extended period of time, there is a connectivity issue to the server. Please check the network connectivity of the Autopower device. Ping the IP of the Autopower device. If it responds, restart mmclient on the Autopower device via ssh (`systemctl restart mmclient`) if you have the rights. Before power cycling the device, ensure if any firewall or similar blocks outgoing messages from the Pi to the ETH/SWITCH network to rule out network issues.
* If the [ACT LED is blinking 4 times in a row (video)](https://github.com/user-attachments/assets/1d8bbb60-62c0-48fc-86a8-1be1338fea19) right after starting the Autopower device, the measurement could not start successfully. Check the USB connection to the power meter and restart the Pi if necessary.

In a working environment, the power meter should [blink periodically (video)](https://github.com/user-attachments/assets/977c5a8e-387a-4c16-ad62-053f82f23812) and the Management UI should show recent data uploads.

The PWR led should not blink and the ACT LED should be either off or have a very faint blink showing disk writes on every measurement. 

If the power meter has been blinking periodically for > 2 minutes and neither the PWR LED is blinking three times in a row nor the ACT LED of the Pi is blinking 4 times in a row, you may assume that the device is measuring correctly.

By default, the recently uploaded data should not be older than 5 to 6 minutes and show up in the Management UI, if you are granted access.

## Ports to open

Strictly speaking, there is no need to forward or open any ports from an external network to the Raspberry Pies. For convenience and easier maintenance reasons, it is beneficial to...

* forward/open port `21092` (TCP) for fall back SSH access (recommended).
* forward/open port `10050` (TCP) for Zabbix monitoring (only used as fallback for monitoring, not necessary, enable for convinience)
