import dash
from dash import Dash, html, dash_table, dcc, Input, Output, callback
import plotly.express as px
import psycopg2 as postgres
import psycopg2.extras as pgextra
import json
import timeago, datetime
import pytz
from tzlocal import get_localzone

dash.register_page(__name__, path="/")

with open('../config/secrets.json') as secrFile:
  secrets = json.load(secrFile)
with open('../config/server_config.json') as configFile:
  config = json.load(configFile)

def createDb():
  return postgres.connect(host=secrets["postgres"]["host"], database=secrets["postgres"]["database"], user=secrets["postgres"]["user"], password=secrets["postgres"]["password"])

def generateClientDashboard():
  with createDb() as pgcon:
    pgcurs = pgcon.cursor(cursor_factory = pgextra.RealDictCursor) # use dict to be able to directly pass to plotly
    pgcurs.execute("SELECT client_uid FROM clients ORDER BY client_uid ASC")
    pgcon.commit()
    tuples = pgcurs.fetchall()

    # generate dashboard for each client
    clientsDivList=[]
    for clientTuple in tuples:
      # get the data of the last 10 minutes.
      pgcurs.execute("SELECT measurement_value, measurement_timestamp FROM measurements, measurement_data WHERE measurements.server_measurement_id = measurement_data.server_measurement_id AND measurement_data.measurement_timestamp > NOW() - INTERVAL '10 minutes' AND measurements.client_uid = %(cluid)s ORDER BY measurement_data.measurement_timestamp", {'cluid': clientTuple["client_uid"]}) # get the data from the last measurement of this client
      pgcon.commit()
      allRecentMsmts = []
      measurementGraph = html.Div("No recent measurements found")

      if pgcurs.rowcount != 0:
        allRecentMsmts = pgcurs.fetchall()

      if allRecentMsmts != []:
        measurementGraph = dcc.Graph(
            figure=px.line(allRecentMsmts, x="measurement_timestamp", y="measurement_value")
          )
      pgcurs.execute("SELECT last_seen FROM clients WHERE client_uid = %(cluid)s LIMIT 1", {'cluid': clientTuple["client_uid"]})
      pgcon.commit()
      activeTimestamp = pgcurs.fetchone()
      # convert timestamp to local time - for now it is not checked if timeago supports timezones
      localtz = get_localzone()
      timestampLocal = activeTimestamp["last_seen"].astimezone(localtz).replace(tzinfo=None)
      timestampString = timeago.format(timestampLocal)
      clientsDivList.append(html.Div(
        [
          html.Div(
            children=[
              html.H2(clientTuple["client_uid"]), # the name of this client
            ]
          ),
          html.Div("Last active " + timestampString, title=str(activeTimestamp["last_seen"])),
          measurementGraph,
        ], className="clientBox"
      )
      )

    pgcurs.execute("SELECT dut_id, dut_name FROM devices_under_test")
    pgcon.commit()
    allDuts = pgcurs.fetchall()
    dutDivList = [html.H2("Devices under test")]
    for dut in allDuts:
      dutDivList.append(html.Div([
        html.H3(dut["dut_name"]),
        dcc.Link(
          children=[
            html.Div("Show aggregated measurements of " + dut["dut_name"])
          ],
          href= dash.get_relative_path("/dut/") + str(dut["dut_id"])
        )
      ]))
    return [html.Div(dutDivList, className="dutDashboardList"), html.Div(clientsDivList, className="clientDashboardList")]


@callback(
    Output("clientsDivList", "children"),
    [Input("updateUi", "n_clicks"),Input("updateIntervalCpnt", "n_intervals")]
)
def updateUi(n_clicks, n_intervals):
    return generateClientDashboard()

layout = html.Div( # main div
    [
      html.Button('Update', id='updateUi', n_clicks=0),
      dcc.Interval(
        id='updateIntervalCpnt',
        interval=10000, # every 10 seconds refresh layout
        n_intervals=0
      ),
      html.Div( # div for clients
        generateClientDashboard(),
        id="clientsDivList"
      )
    ]
)
