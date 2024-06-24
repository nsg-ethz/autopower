# Plot data from postgres database via plotlyimport dash
from gevent import monkey
monkey.patch_all()
# We are not yet 100% sure that the postgres connection is threadsafe, but enabling for now
from psycogreen.gevent import patch_psycopg
patch_psycopg()

import dash
from dash import Dash, html, dcc

# start dash app
app = Dash(__name__, use_pages=True, url_base_pathname='/plotter/')
app.layout = html.Div([
        html.Header([
          dcc.Link(
            children=[
              html.H1("Autopower Plotter")
            ],
            href=dash.get_relative_path("/")
          ),
          html.Div([
            html.A("Go to Management", href="/"),
            html.Hr()
          ])
        ]),
        dash.page_container
])

server = app.server