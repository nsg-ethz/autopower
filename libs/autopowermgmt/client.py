import json
import time

import jwt

from autopowermgmt.proto import api_pb2 as pb2
from autopowermgmt.proto import api_pb2_grpc as pb2_grpc


class AutopowerClient:
    """
    Thin gRPC wrapper around CMeasurementApi.
    Every request is automatically authenticated.
    """

    def __init__(self, channel, mgmt_id: str, mgmt_secret: str):
        self.stub = pb2_grpc.CMeasurementApiStub(channel)
        self.mgmt_id = mgmt_id
        self.mgmt_secret = mgmt_secret

    # -------------------------
    # auth
    # -------------------------

    def _jwt(self) -> str:
        token = jwt.encode(
            {
                "mgmtId": self.mgmt_id,
                "iat": int(time.time()),
            },
            self.mgmt_secret,
            algorithm="HS256",
        )

        return token.decode() if isinstance(token, bytes) else token

    def _auth(self, msg):
        msg.mgmtId = self.mgmt_id
        msg.pw = self._jwt()
        return msg

    # -------------------------
    # single execution gate (IMPORTANT)
    # -------------------------

    def _call(self, rpc, msg):
        return rpc(self._auth(msg))

    # -------------------------
    # device control
    # -------------------------

    def start(
        self, device_uid: str, sampling_int: int, upload_interval: int, pp_device: str
    ):
        settings = pb2.mgmtMsmtSettings()
        settings.clientUid = device_uid
        settings.ppDevice = pp_device
        settings.ppSamplingInterval = str(sampling_int)
        settings.uploadIntervalMin = upload_interval

        self._call(self.stub.setMsmtSttings, settings)

        req = pb2.mgmtRequest()
        req.clientUid = device_uid
        req.msgType = pb2.START_MEASUREMENT

        return self._call(self.stub.issueRequestToClient, req)

    def stop(self, device_uid: str):
        req = pb2.mgmtRequest()
        req.clientUid = device_uid
        req.msgType = pb2.STOP_MEASUREMENT

        return self._call(self.stub.issueRequestToClient, req)

    def ping(self, device_uid: str):
        req = pb2.mgmtRequest()
        req.clientUid = device_uid
        req.msgType = pb2.INTRODUCE_SERVER

        return self._call(self.stub.issueRequestToClient, req)

    def pp_devices(self, device_uid: str):
        req = pb2.mgmtRequest()
        req.clientUid = device_uid
        req.msgType = pb2.REQUEST_AVAILABLE_PP_DEVICE

        resp = self._call(self.stub.issueRequestToClient, req)
        return json.loads(resp.msg) if resp.msg else {}

    def status(self, device_uid: str):
        req = pb2.mgmtRequest()
        req.clientUid = device_uid
        req.msgType = pb2.REQUEST_MEASUREMENT_STATUS

        resp = self._call(self.stub.issueRequestToClient, req)
        return json.loads(resp.msg) if resp.msg else {}
