# Autopower management

This application is used to manage autopower clients via web ui. You can start and stop measurements and manage basic tasks of the server side database via UI. 
In order to use the application, you must enable some kind of authentication as there are no inbuilt user accounts or similar. We use Shibboleth, but you may also use HTTP basic auth.

## Authentication

To setup Shibboleth, please checkout these links to set up authentification: 
* https://help.switch.ch/aai/docs/shibboleth/SWITCH/3.4/sp/deployment/
* https://help.switch.ch/aai/docs/shibboleth/SWITCH/2.6/sp/deployment/?os=ubuntu
* https://help.switch.ch/aai/docs/shibboleth/SWITCH/2.6/sp/deployment/configuration.html?os=ubuntu&hostname=yourhost.example.org

**Note**: Be sure to set the e-mail as required attribute on https://rr.aai.switch.ch/

## Configuring the webserver

**Note:** These instructions are not yet fully tested and to be considered WIP.

Both, Management UI and Plotter use apache2 via `mod_proxy` as proxy before a gunicorn server. You may also use `nginx` or any other server of your liking - but this setup is not tested.

To set up the management ui:
* Create a virtual environment and install the requirements via pip in this management folder.
* Install apache
* Copy the autopower-management.conf file to `/etc/apache2/sites-available/`
* Edit `/etc/apache2/sites-availabla/autopower-management.conf` to fit to your authentication method. You may for example want to add your e-mail address to allow your account to log in via shibboleth.
* Enable mod proxy via `a2enmod proxy` (Not yet tested)
* Enable the autopower-management site `a2ensite autopower-management.conf`
* Copy the `../config/web_config.json.example` file to `../config/web_config.json` and the `../web_secrets.json.example` file to `../web_secrets.json`
* Edit the `web_config.json` and `web_secrets.json` files to fit your setup. Note: You need to set a secret key for management in `web_secrets.json` and put the hash into `secrets.json` file (key allowedMgmtClients). You can create the hash by running cli.py in the server directory with the `--createpassword` argument.
* Set the permissions to both config files such that `www-data` or the user under which you plan to run the management ui under has read access.
* Copy the apmanagement.service file to `/etc/systemd/system/` and edit it to your liking (if needed)
* Start the apmanagement service via systemd `systemctl start apmanagement` and enable autostart `systemctl enable apmanagement`