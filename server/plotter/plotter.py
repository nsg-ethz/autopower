# Plot data from postgres database via plotlyimport dash
import dash
from dash import Dash, html, dcc


if __name__ == '__main__':

    # start dash app
    app = Dash(__name__, use_pages=True)
    app.layout = html.Div([
        html.Header(
            dcc.Link(
                children=[
                    html.H1("Autopower management")
                ],
                href="/"
            )
        ),
        dash.page_container
    ])

    app.run(debug=True, use_reloader=False)  # Reloader is disabled, since otherwise we have two distinct client managers: Flask, on which Dash depends on would otherwise just run the program twice. See https://stackoverflow.com/questions/25504149/why-does-running-the-flask-dev-server-run-itself-twice
