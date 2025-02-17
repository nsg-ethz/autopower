# Deploying an Autopower device

This document serves as writeup for deployment of an autopower device in the data center. Before an autopower device can be deployed, it should have been registered and tested at least once during flashing (initial setup). We assume this has been the case.

## Hardware setup
1. Plug in the power meter in a USB port
2. Set up network access for this autopower device. **Note:** Dual Stack (IPv4 and IPv6) is preferred. The devices should also work behind NAT, or IPv4 only networks, however it is preferrable to have both protocols enabled. By default, the devices get their adresses via DHCP. Custom/other configuration needs to be done on the operating system level during initial setup.
3. Connect the Ethernet cable to the Pi
4. Connect the Power Meter to the device under test
5. Connect the Raspberry Pi to power

After some time, the Raspberry Pi should register and show that it is measuring in the Management UI.
If the red PWR LED is blinking three times in a row periodically for an extended period of time, there is a connectivity issue to the server. Please check the network connectivity of the Autopower device.
If the ACT LED is blinking 4 times in a row right after starting the Autopower device, the measurement could not start successfully. Check the connection to the power meter and restart the Pi if necessary.

In a working environment, the power meter should blink periodically and the Management UI should show recent data uploads. By default, the recently uploaded data should not be older than 5 to 6 minutes. 
