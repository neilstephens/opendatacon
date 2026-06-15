import json
import odc

Trace = 0
Debug = 1
Info = 2
Warn = 3
Error = 4
Critical = 5


class SimPortClass:
     def __init__(self, odcportguid, objectname):
          self.guid = odcportguid
          self.objectname = objectname
          self.config = {}
          odc.log(self.guid, Info, "EverythingPyPort init for {}".format(objectname))

     def Config(self, MainJSON, OverrideJSON):
          self.config = json.loads(MainJSON) if len(MainJSON) else {}
          if len(OverrideJSON):
               self.config.update(json.loads(OverrideJSON))
          odc.log(self.guid, Debug, "EverythingPyPort config loaded")

     def Operational(self):
          odc.log(self.guid, Info, "EverythingPyPort operational")

     def Enable(self):
          odc.log(self.guid, Info, "EverythingPyPort enabled")

     def Disable(self):
          odc.log(self.guid, Info, "EverythingPyPort disabled")

     def EventHandler(self, EventType, Index, Time, Quality, Payload, Sender):
          if EventType == "ControlRelayOutputBlock":
               val = "1" if (Index % 2 == 0) else "0"
               odc.PublishEvent(self.guid, "Binary", 0, "|ONLINE|", val)
          elif EventType == "AnalogOutputInt16":
               odc.PublishEvent(self.guid, "Analog", 0, "|ONLINE|", str(Payload))
          return True

     def TimerHandler(self, TimerId):
          return

     def RestRequestHandler(self, url, content):
          return json.dumps({"Port": self.objectname, "Status": "OK", "URL": url})
