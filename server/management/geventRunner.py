from gevent import monkey
monkey.patch_all()
from psycogreen.gevent import patch_psycopg
patch_psycopg()
import socket
import gevent
# beta: Start gevent non blocking via gunicorn geventRunner:application --worker-class gevent --workers 8 --threads 2

if socket.socket is gevent.socket.socket:
    # https://gist.github.com/craig-rueda/c5de73b43252e5c6d94cd87dab443f50

    import grpc.experimental.gevent
    grpc.experimental.gevent.init_gevent()

from app import app as application
