# Plotter

This folder contains the plotting application. This is based on dash + plotly.

### Plotting setup

Dash is not in the Debian repositories, thus you need to install it via pip.

Create a virtual environment:
`python3 -m venv venv`

Enable the virtual environment:
`source venv/bin/activate`
Install all prequesits as follows:
`pip3 install -r requirements.txt`

Now you can start the Plotter by running `python3 plotter.py`.

## Plotting setup via apache

See ../management/ and adapt the config files respectively. The service file is applotter.service and the apache2 config file is autopower-plotter.conf