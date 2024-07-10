import dash
from dash import Dash, html, dash_table, dcc, Input, Output, callback
import dash_ag_grid as dag
import dash_datetimepicker
import plotly.express as px
import plotly.graph_objects as go
import pandas as pd
import psycopg2 as postgres
import psycopg2.extras as pgextra
import json
import logging
from datetime import datetime, date, time
from tzlocal import get_localzone
from threading import Lock
from concurrent.futures import ThreadPoolExecutor

logging.basicConfig(level=logging.INFO)

dash.register_page(__name__, path_template="/dut/<dut_id>")

def createDb():
    with open('../config/secrets.json') as secrFile:
      secrets = json.load(secrFile)
    pgcon = postgres.connect(host=secrets["postgres"]["host"], database=secrets["postgres"]["database"], user=secrets["postgres"]["user"], password=secrets["postgres"]["password"])
    pgcurs = pgcon.cursor()
    # getMmtAggregateOfDut: (msmt_ids, bin_interval, bin_start_timestamp, measurement_start_timestamp, end_timestamp)
    prepare_ctxt = """
        PREPARE getMmtPtsBinned(VARCHAR(255), INT, TIMESTAMP, TIMESTAMP, TIMESTAMP) AS
          SELECT measurement_bin::timestamp AS measurement_timestamp, AVG(measurement_value) AS measurement_value FROM (
            SELECT 
              measurement_value,
              date_bin($1::interval, measurement_timestamp, $3) AS measurement_bin
            FROM measurement_data
            WHERE
              server_measurement_id = $2
              AND measurement_data.measurement_timestamp BETWEEN $4 AND $5 
          ) AS binned GROUP BY measurement_timestamp ORDER BY measurement_timestamp ASC;
    """
    # set time zone to python time zone
    pgcurs.execute("SET TIME ZONE %(tz)s", {"tz": get_localzone().key})
    # run prepare context
    pgcurs.execute(prepare_ctxt)
    pgcon.commit()
    return pgcon


# Create functions if needed (under lock)
# create or replace function needs to be locked: https://stackoverflow.com/questions/40525684/tuple-concurrently-updated-when-creating-functions-in-postgresql-pl-pgsql
with createDb() as pgcon:
    ps = pgcon.cursor()

    ps.execute("""
    BEGIN;
            SELECT pg_advisory_xact_lock(141421356237);
            CREATE OR REPLACE FUNCTION measurementsOfThisDut(chosen_dut INT) RETURNS TABLE (server_measurement_id INT, client_uid VARCHAR(255)) LANGUAGE plpgsql AS $$
              BEGIN
              RETURN QUERY (
                SELECT measurements.server_measurement_id, measurements.client_uid FROM measurements, runs WHERE measurements.run_id = runs.run_id AND runs.dut_id = chosen_dut
              );
            END; $$;
    COMMIT;
    BEGIN;
          SELECT pg_advisory_xact_lock(934810324982);
          CREATE OR REPLACE FUNCTION getMmtAggregateOfDut(INT[], VARCHAR(255), TIMESTAMP, TIMESTAMP, TIMESTAMP) RETURNS TABLE (measurement_value_out NUMERIC, measurement_timestamp_out TIMESTAMP WITH TIME ZONE) LANGUAGE plpgsql AS $$
          BEGIN
          RETURN QUERY (
            WITH
            binnedMeasurements AS ( -- bin all measurements of given ids into bins of given minutes starting at given time start point. Retain the server_measurement_id to later on add together the two averages per bin
                SELECT measurement_value,
                        date_bin($2::interval, measurement_timestamp, $3) AS measurement_bin,
                        server_measurement_id
                FROM    measurement_data
                WHERE   server_measurement_id = ANY ($1)
                AND    measurement_data.measurement_timestamp BETWEEN $4 AND $5
            ),
            avgdMeasurementsPerMmId AS ( -- average the value per client and measurement_bin
                SELECT   measurement_bin,
                        AVG(measurement_value) AS measurement_avg
                FROM     binnedMeasurements
                GROUP BY measurement_bin, server_measurement_id
            ) 
            SELECT   SUM(measurement_avg) AS measurement_value_out, -- sum up the selected, averaged results
                    measurement_bin      AS measurement_timestamp_out
            FROM     avgdMeasurementsPerMmId
            GROUP BY measurement_timestamp_out
            ORDER BY measurement_timestamp_out ASC
          );
        END; $$;
    COMMIT;
    BEGIN;
      SELECT pg_advisory_xact_lock(31415926535);
      CREATE OR REPLACE FUNCTION getMsmtsByDow(INT[], VARCHAR(255), TIMESTAMP, TIMESTAMP) RETURNS TABLE (measurement_value NUMERIC, measurement_time TIME, measurement_dow DOUBLE PRECISION, measurement_timestamp TIMESTAMP WITH TIME ZONE) LANGUAGE plpgsql AS $$
          BEGIN
          RETURN QUERY (
            SELECT
              measurement_value_out AS measurement_value,
              CAST(measurement_timestamp_out AS TIME) AS measurement_time,
              DATE_PART('isodow', measurement_timestamp_out) AS measurement_dow, -- isodow has 1 = Monday
              measurement_timestamp_out AS measurement_timestamp
            FROM
              getMmtAggregateOfDut($1,$2, DATE_TRUNC('week',$3),$3,$4)
            ORDER BY DATE_PART('dow', measurement_timestamp_out), CAST(measurement_timestamp_out AS TIME) ASC
          );
          END; $$;
    COMMIT;
    BEGIN;
      SELECT pg_advisory_xact_lock(348091340808);
      CREATE OR REPLACE FUNCTION getMsmtsByHour(INT[], VARCHAR(255), TIMESTAMP, TIMESTAMP) RETURNS TABLE (measurement_value NUMERIC, measurement_time TIME, measurement_timestamp TIMESTAMP WITH TIME ZONE) LANGUAGE plpgsql AS $$
        BEGIN
        RETURN QUERY (
          SELECT
            measurement_value_out AS measurement_value,
            CAST(measurement_timestamp_out AS TIME) AS measurement_time,
            measurement_timestamp_out AS measurement_timestamp
          FROM
            getMmtAggregateOfDut($1,$2,DATE_TRUNC('day', $3),$3,$4)
          ORDER BY CAST(measurement_timestamp_out AS TIME) ASC
        );
        END; $$;
    COMMIT; 
    """)
    pgcon.commit()


def layout(dut_id=None, **kwargs):
    dutContent = ""
    if not dut_id:
        dutContent = "Got invalid dut_id! Please check parameter!"
    else:
        try:
            int(dut_id)
            dutContent = layoutDutPage(int(dut_id))
        except ValueError:
            dutContent = "Got non integer dut_id. Please check parameter for typos."
    with createDb() as pgcon:
        availableDutsCurs = pgcon.cursor(cursor_factory=pgextra.RealDictCursor)  # use dict to be able to directly pass to plotly
        availableDutsCurs.execute("SELECT dut_id AS value, dut_name AS label FROM devices_under_test ORDER BY dut_id ASC")
        pgcon.commit()
        allDuts = availableDutsCurs.fetchall()
    return html.Div([
        dcc.Location(id="dutLocation"),
        dcc.Dropdown(allDuts, value=dut_id, id="dutIdSelect"),
        html.Button('Change DUT', id='dutUpdateUi', n_clicks=0),
        html.Div(dutContent, id="dutContent")]
    )


@callback(
    Output("dutLocation", "pathname"),
    [Input("dutIdSelect", "value"), Input("dutUpdateUi", "n_clicks")],
    prevent_initial_call=True
)
def changePage(dut_id, n_clicks):
    if n_clicks > 0:
        return dash.get_relative_path("/dut/") + str(dut_id)
    else:
        return dash.no_update


def layoutDutPage(dut_id=None):
    # select all measurements of this dut
    if not isinstance(dut_id, int):
        return html.Div("Got invalid dut_id. Please check the parameter of the link.")
    with createDb() as pgcon:
        layoutCurs = pgcon.cursor(cursor_factory=pgextra.RealDictCursor)  # use dict to be able to directly pass to plotly
        timingCurs = pgcon.cursor(cursor_factory=pgextra.RealDictCursor)  # cursor to get min/max timings

        layoutCurs.execute("SELECT msmts_of_this_dut.server_measurement_id, measurements.client_uid, measurements.shared_measurement_id FROM measurementsOfThisDut(%(dutId)s) AS msmts_of_this_dut, measurements WHERE measurements.server_measurement_id = msmts_of_this_dut.server_measurement_id GROUP BY measurements.shared_measurement_id, msmts_of_this_dut.server_measurement_id, measurements.client_uid", {"dutId": dut_id})
        pgcon.commit()
        allMsmts = layoutCurs.fetchall()
        allMsmtIds = []
        num = 0
        for msmt in allMsmts:
            allMsmtIds.append(msmt["server_measurement_id"])
            timingCurs.execute("SELECT MAX(measurement_timestamp)::timestamp AS last_ts, MIN(measurement_timestamp)::timestamp AS start_ts FROM measurement_data WHERE server_measurement_id = %(srvmmid)s", {'srvmmid': msmt["server_measurement_id"]})
            timings = timingCurs.fetchone()
            allMsmts[num]["last_ts"] = timings["last_ts"]
            allMsmts[num]["start_ts"] = timings["start_ts"]
            num = num + 1


        if allMsmtIds == []:
            return html.Div("No measurements found for this dut!")

        # get the name of this dut
        layoutCurs.execute("SELECT dut_name FROM devices_under_test WHERE dut_id = %(dutId)s", {"dutId": dut_id})
        deviceName = layoutCurs.fetchone()["dut_name"]
        layout = html.Div(  # main div
            [
                html.Div([  # div for measurements table
                    html.H2(deviceName, id="dutName"),
                    html.H3(dut_id, id="dutId"),  # attention: This is also used to get data into the callback
                    dash_table.DataTable(allMsmts, id="dutMsmtTable")
                ]),
                html.Div(
                    [
                        dcc.Dropdown(allMsmtIds, id="dutMsmtDropdown", multi=True),
                        html.Div([
                            dash_datetimepicker.DashDatetimepicker(
                                id="dutMsmtTimeSelect",
                                locale="de",
                            ),
                            dcc.Dropdown(
                                ["1 second", "2 seconds", "5 seconds", "10 seconds", "1 minute", "5 minutes", "15 minutes", "30 minutes", "1 hours", "3 hours", "6 hours", "12 hours", "1 day", "2 days"],
                                id="dutMsmtBinSelect",
                                value="30 minutes"
                            )
                        ], id="dutStartEndDiv"),
                        html.Button('Build graphs', id='dutShowMsmtBtn', disabled=True, n_clicks=0)
                    ]
                ),
                html.Button("Download summed data", id="dutRawDownloadButton", hidden=True, n_clicks=0),
                html.Div("Please select an id and time frame to show the graph", id="dutPowergraph"),
                dcc.Loading(id="dutLoadingPowergraph", type="circle", children=html.Div(id="dutLoadinPgOutput")),
                html.Div(id="dutHourlyPlot"),
                html.Div(id="dutDailyPlot"),
                dag.AgGrid(
                    id="dutRawDataTable",
                    rowData=[],
                    style={
                        'display': 'none',
                    },
                    columnDefs=[
                        {"field": "measurement_timestamp"},
                        {"field": "measurement_value"}
                    ],
                    csvExportParams={
                        "fileName": deviceName + ".csv"
                    }
                )
            ])
    return layout

# TODO: Debug callback missing


@callback(
    [Output("dutMsmtTimeSelect", "startDate"), Output("dutMsmtTimeSelect", "endDate"), Output("dutLoadingPowergraph", "loading_state", allow_duplicate=True)],
    Input("dutMsmtDropdown", "value"),
    prevent_initial_call=True
)
def updateStartEndSelection(mmtids):
    with createDb() as pgcon:
        updateStartEndCurs = pgcon.cursor(cursor_factory=pgextra.RealDictCursor)
        updateStartEndCurs.execute("""
      SELECT MAX(lower_measurement_timestamp) AS min_ts, MIN(upper_measurement_timestamp) AS max_ts FROM (
          SELECT MIN(measurement_timestamp) AS lower_measurement_timestamp, MAX(measurement_timestamp) AS upper_measurement_timestamp
          FROM measurement_data
          WHERE server_measurement_id = ANY (%(mmtids)s)
          GROUP BY server_measurement_id
      ) AS idMinMaxGroups;
      """, {"mmtids": mmtids})
        res = updateStartEndCurs.fetchone()
        # TODO: Handle min/max not present
        return res["min_ts"], res["max_ts"], {"component_name": "dutPowergraph", "is_loading": True}


@callback(
    Output("dutShowMsmtBtn", "disabled"),
    [Input("dutMsmtTimeSelect", "startDate"), Input("dutMsmtTimeSelect", "endDate"), Input("dutMsmtDropdown", "value"), Input("dutMsmtBinSelect", "value")]
)
def showMsmtButton(start, end, mmtids, mmtBin):
    if start != None and end != None and (not (mmtids == [] or mmtids == None)) and mmtBin != None:
        return False
    else:
        return True


@callback(
    [Output("dutPowergraph", "children"), Output("dutRawDataTable", "rowData"), Output("dutRawDownloadButton", "hidden"), Output("dutShowMsmtBtn", "n_clicks"), Output("dutHourlyPlot", "children"), Output("dutDailyPlot", "children"), Output("dutLoadingPowergraph", "loading_state", allow_duplicate=True)],
    [Input("dutMsmtBinSelect", "value"), Input("dutMsmtTimeSelect", "startDate"), Input("dutMsmtTimeSelect", "endDate"), Input("dutMsmtDropdown", "value"), Input("dutShowMsmtBtn", "n_clicks")],
    prevent_initial_call=True
)
def showMsmtPlot(binTime, startDate, endDate, msmt_ids, n_clicks):
    if n_clicks > 0:
        with createDb() as pgcon:
            # Using psycopg2 may give a warning for not being supported (as of May 2024 pandas doesn't officially support psycopg2) even though it works: https://github.com/pandas-dev/pandas/issues/45660
            allRecentMsmts = pd.read_sql_query(sql="SELECT measurement_value_out AS measurement_value, measurement_timestamp_out::timestamp AS measurement_timestamp FROM getMmtAggregateOfDut (%(mmtids)s, %(bintime)s, %(startts)s, %(startts)s, %(endts)s)", params={"bintime": binTime, "mmtids": msmt_ids, "startts": startDate, "endts": endDate}, con=pgcon)
            allRecentMsmts["measurement_value"] = allRecentMsmts["measurement_value"].div(1000)  # convert mW into W
            if allRecentMsmts.empty:
                return "Didn't get any data. Please check the timeframe and id.", [], True, 0, "", "", {"component_name": "dutPowergraph", "is_loading": False}
            else:
                # compose the graph together
                # first add the aggregated measurements
                def buildTimeseriesPlot():
                    plt = go.Figure(layout_title_text="Aggregated measurements")
                    plt.add_trace(go.Scatter(x=allRecentMsmts["measurement_timestamp"], y=allRecentMsmts["measurement_value"], name="Summed measurement"))
                    if True:  # add the split measurements
                        def getMsmtVals(msmt_id):
                            msmtDf = pd.read_sql_query(sql="EXECUTE getMmtPtsBinned (%(bintime)s, %(srvmsmtid)s, %(startts)s,%(startts)s, %(endts)s)", params={"bintime": binTime, "srvmsmtid": msmt_id, "startts": startDate, "endts": endDate}, con=createDb())
                            msmtDf["measurement_value"] = msmtDf["measurement_value"].div(1000)  # convert mW to W
                            return go.Scatter(x=msmtDf["measurement_timestamp"], y=msmtDf["measurement_value"], name="Measurement id " + str(msmt_id))
                        allMsmtsForThisId = list(map(getMsmtVals, msmt_ids))
                        plt.add_traces(allMsmtsForThisId)
                    
                    # Add x axis and y axis titles
                    plt.update_layout(xaxis_title="Time", yaxis_title="Measured power [W]")
                    return plt
                
                def buildHourlyPlot():
                    # copy the dataframe
                    hourlyMsmts = pd.read_sql_query(sql="SELECT measurement_value, measurement_time FROM getMsmtsByHour (%(mmtids)s, %(bintime)s, %(startts)s, %(endts)s)", params={"bintime": binTime, "mmtids": msmt_ids, "startts": startDate, "endts": endDate}, con=createDb())
                    # and update the date to only show the minutes
                    hourlyMsmts['measurement_value'] = hourlyMsmts['measurement_value'].div(1000)
                    # naively map the calculate the time to one day close to the start of the unix epoch. This seems like a hack. Alternatively follow something like:
                    # seconds_per_day = 24*60*60
                    # bin_size = 5*60 # 5-minute bins
                    # time_s = ((pd.to_datetime(df.index).astype(int) / 10**9)%seconds_per_day/bin_size).astype(int)
                    # df['bin-time'] = pd.to_datetime(time_s*bin_size, unit='s')

                    hourlyMsmts['measurement_time'] = hourlyMsmts['measurement_time'].map(lambda t: datetime(1970, 1, 5) + (datetime.combine(date.min, t) - datetime.min))
                    hourlyMsmts.sort_values(by="measurement_time", inplace=True)
                    # calculate median
                    medianHourlyMsmts = hourlyMsmts.groupby(by=["measurement_time"], as_index=False).median()
                    # now calculate the hourly plot

                    # create opacity slider: https://stackoverflow.com/a/74317122
                    opacitySlider = {
                        'active': 50,
                        'currentvalue': {'prefix': 'Opacity: '},
                        'steps': [{
                            'value': step / 100,
                            'label': f'{step}%',
                            'visible': True,
                            'execute': True,
                            'method': 'restyle',
                            'args': [{'opacity': step / 100}, [0]]
                        } for step in range(101)]
                    }

                    hourlyPlot = go.Figure(layout_title_text="Daily power")
                    hourlyPlot.add_trace(
                        go.Scatter(x=hourlyMsmts["measurement_time"], y=hourlyMsmts["measurement_value"], mode='markers', name="Averaged datapoints")
                    )
                    hourlyPlot.add_trace(
                        go.Scatter(x=medianHourlyMsmts["measurement_time"], y=medianHourlyMsmts["measurement_value"], name="Median")
                    )
                    hourlyPlot.update_layout(xaxis_title="Time of day", yaxis_title="Measured power [W]", sliders=[opacitySlider])
                    # TODO: This feels wrong: this won't work for higher sampling rates (https://gitlab.ethz.ch/nsg/students/projects/2024/ba-2024_auto-power/-/issues/23)
                    hourlyPlot.update_xaxes(tickformat="%H:%M")
                    hourlyPlot.update_traces(marker=dict(opacity=0.5,), selector=dict(mode='markers'))
                    return hourlyPlot
                def buildDailyPlot():
                    # daily plot
                    dailyMsmts = pd.read_sql_query(sql="SELECT measurement_value, measurement_dow, measurement_time, measurement_timestamp FROM getMsmtsByDow (%(mmtids)s, %(bintime)s, %(startts)s, %(endts)s)", params={"bintime": binTime, "mmtids": msmt_ids, "startts": startDate, "endts": endDate}, con=createDb())

                    dailyMsmts = dailyMsmts.apply(
                        (lambda row:
                        pd.Series(
                            [row["measurement_value"] / 1000, (datetime(1970, 1, 4 + int(row["measurement_dow"])) + (datetime.combine(date.min, row["measurement_time"]) - datetime.min))],
                            index=["measurement_value", "measurement_reduced_ts"]
                        )
                        ), axis="columns")
                    dailyMsmts.sort_values(by="measurement_reduced_ts", inplace=True)
                    # calculate median
                    medianDailyMsmts = dailyMsmts.groupby("measurement_reduced_ts", as_index=False).median()
                    dailyPlot = go.Figure(layout_title_text="Weekly power")
                    dailyPlot.add_trace(
                        go.Scatter(x=dailyMsmts["measurement_reduced_ts"], y=dailyMsmts["measurement_value"], mode='markers', name="Averaged datapoints")
                    )
                    dailyPlot.add_trace(
                        go.Scatter(x=medianDailyMsmts["measurement_reduced_ts"], y=medianDailyMsmts["measurement_value"], name="Median")
                    )


                    # create opacity slider: https://stackoverflow.com/a/74317122
                    opacitySlider = {
                        'active': 50,
                        'currentvalue': {'prefix': 'Opacity: '},
                        'steps': [{
                            'value': step / 100,
                            'label': f'{step}%',
                            'visible': True,
                            'execute': True,
                            'method': 'restyle',
                            'args': [{'opacity': step / 100}, [0]]
                        } for step in range(101)]
                    }

                    dailyPlot.update_layout(xaxis_title="Weekday", yaxis_title="Measured power [W]", sliders=[opacitySlider])
                    dailyPlot.update_xaxes(tickformat="%A, %H:%M")
                    dailyPlot.update_traces(marker=dict(opacity=0.5,), selector=dict(mode='markers'))
                    return dailyPlot

                with ThreadPoolExecutor(max_workers=3) as executor:
                    tsPlotFuture = executor.submit(buildTimeseriesPlot)
                    hourlyPlotFuture = executor.submit(buildHourlyPlot)
                    dailyPlotFuture = executor.submit(buildDailyPlot)

                    return dcc.Graph(figure=tsPlotFuture.result()), allRecentMsmts.to_dict('records'), False, 0, dcc.Graph(figure=hourlyPlotFuture.result()), dcc.Graph(figure=dailyPlotFuture.result()), {"component_name": "dutPowergraph", "is_loading": False}  # 0 is for n clicks reset
    else:
        return "Please select an id and time frame to show the graphs", [], True, 0, "", "", {"component_name": "dutPowergraph", "is_loading": False}


@callback(
    [Output("dutMsmtDrop", "options"), Output("dutMsmtDrop", "value"), Output("dutMsmtTable", "data")],
    [Input("dutId", "children"), Input("dutMsmtDrop", "value")],
    prevent_initial_call=True
)
def dutUpdate(dut_id, previous_msmt):
    with createDb() as pgcon:
        dutUdCurs = pgcon.cursor(cursor_factory=pgextra.RealDictCursor)
        dutUdCurs.execute("SELECT msmts_of_this_dut.server_measurement_id, measurements.client_uid, MAX(measurement_timestamp)::timestamp AS last_ts, MIN(measurement_timestamp)::timestamp AS start_ts FROM measurementsOfThisDut(%(dutId)s) AS msmts_of_this_dut, measurement_data, measurements WHERE measurement_data.server_measurement_id = msmts_of_this_dut.server_measurement_id AND measurements.server_measurement_id = msmts_of_this_dut.server_measurement_id GROUP BY msmts_of_this_dut.server_measurement_id, measurements.client_uid", {"dutId": dut_id})
        pgcon.commit()
        allMsmts = dutUdCurs.fetchall()
        if allMsmts.empty:
            return html.Div("No measurements found for this device!")

        allMsmtIds = []
        for msmt in allMsmts:
            allMsmtIds.append(msmt["server_measurement_id"])

        selectedMsmt = allMsmtIds[0]
        # if previous_msmt in allMsmtIds:
        #    # do not jump if the currently selected measurement is still available. Else return to the biggest one
        #    selectedMsmt = previous_msmt
        return allMsmtIds, selectedMsmt, allMsmts


@callback(
    Output("dutRawDataTable", "exportDataAsCsv"),
    Input("dutRawDownloadButton", "n_clicks")
)
def doCsvExport(n_clicks):
    if n_clicks:
        return True
    else:
        return False
