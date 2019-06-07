import types
import json
from datetime import datetime
import odc

Trace = 0
Debug = 1
Info = 2
Warn = 3
Error = 4
Critical = 5

class SimPortClass:
    ''' Our class to handle an ODC Port. We must have __init__, ProcessJSONConfig, Enable, Disable, EventHander defined, as they will be called by our c/c++ code.
    The c/c++ code also defines some methods on this class, which we can call, as well as possibly some global functions we can call.
    The class methods are part of odc. We currently have odc.log and odc.publishevent.
    '''
  #  class LogLevel:
   #     Trace   = 0
   #     Debug   = 1
    #    Info    = 2
    #    Warn    = 3
    #    Error   = 4
    #    Critical= 5
    
    enabled = False # class variable shared by all instances

    def __init__(self, odcportguid, objectname):
        self.objectname = objectname    # So we can find this instance later to call appropriate PublishEvent method
        self.guid = odcportguid # So that when we call an odc method, we can work out which odcport to hand it too. 
        odc.log(self.guid,Debug,"SimPortClass Init Called - {}".format(objectname))

    def Config(self, MainJSON, OverrideJSON):
        """ The JSON values should be passed as strings, which we then load into a dictionary for processing"""
        odc.log(self.guid,Debug,"Passed Main JSON Config information - {}".format(MainJSON))
        odc.log(self.guid,Debug,"Passed Override JSON Config information - {} , {}".format(OverrideJSON, len(OverrideJSON)))

        # Strip comments from JSON
        # https://stackoverflow.com/questions/43302828/how-to-remove-comment-lines-from-a-json-file-in-python


        # Load JSON into Dicts
        Main = {}
        Override = {}
        if len(MainJSON) != 0:
            Main = json.loads(MainJSON)   
        if len(OverrideJSON) != 0:
            Override = json.loads(OverrideJSON)

       # odc.log(self.guid,LogLevel.Debug,MainJSON)
       # odc.log(self.guid,LogLevel.Debug,OverrideJSON)
        
    def Enable(self):
        odc.log(self.guid,Trace,"Enable Method"+ datetime.now().isoformat(" "))
        for d in dir():
            odc.log(self.guid,2,"Dir "+ d)
        enabled = True

    def Disable(self):
        odc.log(self.guid,Error,"Disabled Method")
        enabled = False

    def EventHandler(self,EventType, Index, Time, Quality, Sender):
       odc.log(self.guid,3,"EventHander: {}, {}".format(Sender, self.guid))