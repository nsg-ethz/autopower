# Autopower client

## Folder content

This folder contains files needed for the client side of the autopower project:

- `deploy.sh` A bash script which runs on the Raspberry Pi to setup the environment. This asks interactively for a device name and sets up all respective binaries, the database, firewall, Zabbix client for monitoring, etc
- `deploy/create_client_cert.sh` A script to generate certificates for the clients. Those need to be signed from a CA trusted by the server to allow encrypted connection.
- `serverIpConfig.sh` A bash script containing configuration parameters like the domain of the autopower server. Edit this to your needs. Also do the same to `deploy/cnConfig.sh` for the CN in the certificates.
- `config/client_config.json.example`: Example config file for the client. This contains setting parameters like the uid of an autopower device.
- `config/secrets.json.example`: Example config file which contains secrets like the path to certificates for the client.

## Deployment

In order to deploy an autopower device, you may need to use a Raspberry Pi 4 - preferred Pi 4 4 GB or more to compile the code. Deployment was tested on a Raspberry Pi 3 B, 3 B+ and 4 B 1 GB is enough.

First compile [our fork of pinpoint](https://github.com/nsg-ethz/pinpoint/tree/feature/skip-workload) from the `skip-workload` branch which contains a flag to skip the workload parameter as shown in the [official pinpoint repository README.md](https://github.com/osmhpi/pinpoint/blob/master/README.md) file and copy the resulting binary to the `bin/` folder. If needed, rename the client to mmclient.

Afterwards compile the autopower client (mmclient) from this repository as described in COMPILING.md and copy the resulting `client` binary to bin/mmclient.

You can also use the precompiled binaries from [GitHub releases](https://github.com/nsg-ethz/autopower/releases).

Now edit the `serverIpConfig.sh` script with the domain and IP of the autopower server and the `deploy/cnConfig.sh` file with the CN (usually the domain) of your server.

### Setting up a Raspberry Pi

First of all, flash an OS to the SD card of the Raspberry Pi. This project was tested with Raspberry Pi OS Lite (64-bit).

1. Download and install the Raspberry Pi Imager from the [official website](https://www.raspberrypi.com/software/). 
2. Select the correct Model and OS in the imager UI. You may need to select `Raspberrypi OS (other)` in the OS selection menu to select the version without desktop.
3. Select `RASPBERRYPI OS LITE (64-BIT)`.
4. Select the SD card to write on
5. Change OS settings in the imager:
  - Set the hostname to the device name used (e.g. autopower1).
  - Set the username `ethditet` and a secure password
  - Set up SSH under services and paste your SSH key (the deploy.sh script also can append one later on)
  - Disable telemetry under options
  - Now save the settings
6. Write the image to the SD card

**Note:** Instead of using the imager, you may also [download the OS image](https://www.raspberrypi.com/software/operating-systems/) and use use dd to flash it: `dd if=/path/to/raspberrypiosLite64bit.img of=/path/to/sdcard bs=4M`. You may need to enable SSH by creating an empty file in the bootfs volume of the SD card called `ssh` manually: `touch /media/bootfs/ssh`. After that, continue with the deploy script as follows.

### Deployment on the Pi

**Note:** This method uses a USB stick, but you can of course also copy the files via SFTP or SCP if you know the IP.

**SSH Keys:** To add a SSH key for SSH connection to the Pi (e.g. if you did not use the Raspberry Pi imager), put your SSH Key into a file called `ssh_key.pub` in the `client/deploy/` folder. This will then be added to each Pis' `authorized_keys` file.

1. Copy the `client` folder of this repository including the compiled binaries (and SSH key if needed) to a USB stick.
2. Plug the USB stick into the Pi
3. Power up the Pi
4. Once the Pi has booted up, connect monitor (and keyboard if needed) to the Pi (the Pi will show the IP address after it has booted as "My IP address is x.x.x.x"). You can also directly use the network e.g. via SSH and the credentials you have set up in the imager if you know the IP via e.g. `ssh ethditet@<autopowerip>`
5. Mount the USB stick e.g. via `sudo mount /dev/sda1 /mnt`
6. Copy the client folder to `/tmp/client`: `cp -r /mnt/client /tmp/client`

### Running the deploy script

1. Change to the /tmp/client directory via `cd /tmp/client`
2. Run the deployment script: `sudo chmod +x deploy.sh && sudo ./deploy.sh`. You are asked to input the name of the device you want to deploy. This will also set the hostname of this device and copy a SSH key to the `ethditet` user. Do **not** disconnect if you are using SSH as this may prevent you from reconnecting before a forced reboot due to the port change of the SSH server and the firewall.
3. After the script has finished, copy the resulting `client.csr` file and the `zabbix_psk.psk` file to the server. If you are using a USB stick, use `cp /etc/mmclient/client_<deviceName>.csr /mnt/<deviceName>.csr` and `cp zabbix_psk.psk /mnt/zabbix_psk_<deviceName>.psk` and then remove zabbix_psk.psk via: `rm zabbix_psk.psk`.

On the server (e.g. in a seperate `certs` folder where you generated the CA signing the certificates of the Pi), sign the client.csr file with e.g. `openssl x509 -req -in <deviceName>.csr -CA ca.cer -CAkey ca.key -CAcreateserial -out <deviceName>.cer -days 365 -sha512` and copy the resulting client.cer file to `/etc/mmclient/client.cer` and `ca.cer` file to `/etc/mmclient/ca.cer` on the Pi. For more information, check the README file in the server folder.
Now, set up Zabbix on the Zabbix Online UI with the PSK from zabbix_psk.psk and the PSK identity `PSK <your_selected_device_name>`. 

To configure PSK encryption and register this agent in the Zabbix frontend:
 - Go to: Data collection → Hosts
 - Select host or add host and click on the `Encryption` tab
 - Fill in the PSK identity (`PSK <device-name>`) and the PSK found in the `zabbix_psk.psk` file
 - Update/add the host IP address in the `Host` tab, under `Interfaces`.  
 
If everything works, the host will register and the `Avaibility` tag will turn green.

Now plug in the power meter and then reboot the Pi. Note that the client will stop attempting to start a measurement if no data is coming in from the power-meter within the first 10 + samplingInterval[ms]/1000 seconds after starting the client or starting a measurement.

Now check if you can access the Pi as described in the next section.

### Accessing the Pi

The Pi is configured to get an IP address via DHCP. To connect via SSH, after setup, use port 21092 (not 22) and the ethditet user: `ssh ethditet@<autopowerip> -p 21092`