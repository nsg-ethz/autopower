from google.protobuf import timestamp_pb2 as _timestamp_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class srvRequestType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    INTRODUCE_SERVER: _ClassVar[srvRequestType]
    START_MEASUREMENT: _ClassVar[srvRequestType]
    STOP_MEASUREMENT: _ClassVar[srvRequestType]
    REQUEST_MEASUREMENT_LIST: _ClassVar[srvRequestType]
    REQUEST_MEASUREMENT_DATA: _ClassVar[srvRequestType]
    REQUEST_MEASUREMENT_STATUS: _ClassVar[srvRequestType]

class clientResponseType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    INTRODUCE_CLIENT: _ClassVar[clientResponseType]
    STARTED_MEASUREMENT_RESPONSE: _ClassVar[clientResponseType]
    STOPPED_MEASUREMENT_RESPONSE: _ClassVar[clientResponseType]
    MEASUREMENT_LIST_RESPONSE: _ClassVar[clientResponseType]
    MEASUREMENT_DATA_RESPONSE: _ClassVar[clientResponseType]
    MEASUREMENT_STATUS_RESPONSE: _ClassVar[clientResponseType]
INTRODUCE_SERVER: srvRequestType
START_MEASUREMENT: srvRequestType
STOP_MEASUREMENT: srvRequestType
REQUEST_MEASUREMENT_LIST: srvRequestType
REQUEST_MEASUREMENT_DATA: srvRequestType
REQUEST_MEASUREMENT_STATUS: srvRequestType
INTRODUCE_CLIENT: clientResponseType
STARTED_MEASUREMENT_RESPONSE: clientResponseType
STOPPED_MEASUREMENT_RESPONSE: clientResponseType
MEASUREMENT_LIST_RESPONSE: clientResponseType
MEASUREMENT_DATA_RESPONSE: clientResponseType
MEASUREMENT_STATUS_RESPONSE: clientResponseType

class nothing(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class cmMCode(_message.Message):
    __slots__ = ("clientUid", "statusCode", "msg")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    STATUSCODE_FIELD_NUMBER: _ClassVar[int]
    MSG_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    statusCode: int
    msg: str
    def __init__(self, clientUid: _Optional[str] = ..., statusCode: _Optional[int] = ..., msg: _Optional[str] = ...) -> None: ...

class srvRequest(_message.Message):
    __slots__ = ("clientUid", "msgType", "requestBody", "requestNo")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    MSGTYPE_FIELD_NUMBER: _ClassVar[int]
    REQUESTBODY_FIELD_NUMBER: _ClassVar[int]
    REQUESTNO_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    msgType: srvRequestType
    requestBody: str
    requestNo: int
    def __init__(self, clientUid: _Optional[str] = ..., msgType: _Optional[_Union[srvRequestType, str]] = ..., requestBody: _Optional[str] = ..., requestNo: _Optional[int] = ...) -> None: ...

class clientResponse(_message.Message):
    __slots__ = ("clientUid", "statusCode", "msgType", "msg", "requestNo")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    STATUSCODE_FIELD_NUMBER: _ClassVar[int]
    MSGTYPE_FIELD_NUMBER: _ClassVar[int]
    MSG_FIELD_NUMBER: _ClassVar[int]
    REQUESTNO_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    statusCode: int
    msgType: clientResponseType
    msg: str
    requestNo: int
    def __init__(self, clientUid: _Optional[str] = ..., statusCode: _Optional[int] = ..., msgType: _Optional[_Union[clientResponseType, str]] = ..., msg: _Optional[str] = ..., requestNo: _Optional[int] = ...) -> None: ...

class clientUid(_message.Message):
    __slots__ = ("uid",)
    UID_FIELD_NUMBER: _ClassVar[int]
    uid: str
    def __init__(self, uid: _Optional[str] = ...) -> None: ...

class msmtSettings(_message.Message):
    __slots__ = ("clientUid", "ppDevice", "ppSamplingInterval", "uploadIntervalMin")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    PPDEVICE_FIELD_NUMBER: _ClassVar[int]
    PPSAMPLINGINTERVAL_FIELD_NUMBER: _ClassVar[int]
    UPLOADINTERVALMIN_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    ppDevice: str
    ppSamplingInterval: str
    uploadIntervalMin: int
    def __init__(self, clientUid: _Optional[str] = ..., ppDevice: _Optional[str] = ..., ppSamplingInterval: _Optional[str] = ..., uploadIntervalMin: _Optional[int] = ...) -> None: ...

class msmtSample(_message.Message):
    __slots__ = ("clientUid", "msmtId", "msmtTime", "msmtContent")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    MSMTID_FIELD_NUMBER: _ClassVar[int]
    MSMTTIME_FIELD_NUMBER: _ClassVar[int]
    MSMTCONTENT_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    msmtId: str
    msmtTime: _timestamp_pb2.Timestamp
    msmtContent: int
    def __init__(self, clientUid: _Optional[str] = ..., msmtId: _Optional[str] = ..., msmtTime: _Optional[_Union[_timestamp_pb2.Timestamp, _Mapping]] = ..., msmtContent: _Optional[int] = ...) -> None: ...

class msmtName(_message.Message):
    __slots__ = ("clientUid", "name")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    name: str
    def __init__(self, clientUid: _Optional[str] = ..., name: _Optional[str] = ...) -> None: ...

class mgmtAuth(_message.Message):
    __slots__ = ("mgmtId", "pw")
    MGMTID_FIELD_NUMBER: _ClassVar[int]
    PW_FIELD_NUMBER: _ClassVar[int]
    mgmtId: str
    pw: str
    def __init__(self, mgmtId: _Optional[str] = ..., pw: _Optional[str] = ...) -> None: ...

class mgmtMsmtSettings(_message.Message):
    __slots__ = ("clientUid", "ppDevice", "ppSamplingInterval", "uploadIntervalMin", "mgmtId", "pw")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    PPDEVICE_FIELD_NUMBER: _ClassVar[int]
    PPSAMPLINGINTERVAL_FIELD_NUMBER: _ClassVar[int]
    UPLOADINTERVALMIN_FIELD_NUMBER: _ClassVar[int]
    MGMTID_FIELD_NUMBER: _ClassVar[int]
    PW_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    ppDevice: str
    ppSamplingInterval: str
    uploadIntervalMin: int
    mgmtId: str
    pw: str
    def __init__(self, clientUid: _Optional[str] = ..., ppDevice: _Optional[str] = ..., ppSamplingInterval: _Optional[str] = ..., uploadIntervalMin: _Optional[int] = ..., mgmtId: _Optional[str] = ..., pw: _Optional[str] = ...) -> None: ...

class mgmtRequest(_message.Message):
    __slots__ = ("clientUid", "msgType", "requestBody", "requestNo", "mgmtId", "pw")
    CLIENTUID_FIELD_NUMBER: _ClassVar[int]
    MSGTYPE_FIELD_NUMBER: _ClassVar[int]
    REQUESTBODY_FIELD_NUMBER: _ClassVar[int]
    REQUESTNO_FIELD_NUMBER: _ClassVar[int]
    MGMTID_FIELD_NUMBER: _ClassVar[int]
    PW_FIELD_NUMBER: _ClassVar[int]
    clientUid: str
    msgType: srvRequestType
    requestBody: str
    requestNo: int
    mgmtId: str
    pw: str
    def __init__(self, clientUid: _Optional[str] = ..., msgType: _Optional[_Union[srvRequestType, str]] = ..., requestBody: _Optional[str] = ..., requestNo: _Optional[int] = ..., mgmtId: _Optional[str] = ..., pw: _Optional[str] = ...) -> None: ...
