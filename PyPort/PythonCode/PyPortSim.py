import types
import json
import sys
from datetime import datetime
import odc

# Logging Levels
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

# Worker Methods used by the Methods called by ODC. They need to be in this section so they are available in the code below.

    def LogTrace(self, message ):
        odc.log(self.guid, Trace, message )

    def LogError(self, message ):
        odc.log(self.guid,Error, message )

    def LogDebug(self, message ):
        odc.log(self.guid, Debug, message )

    def LogInfo(self, message ):
        odc.log(self.guid,Info, message )

    def LogWarn(self, message ):
        odc.log(self.guid, Warn, message )

    def LogCritical(self, message ):
        odc.log(self.guid,Critical, message )


# Mandatory Methods that are called by ODC PyPort

    def __init__(self, odcportguid, objectname):
        self.objectname = objectname    # So we can find this instance later to call appropriate PublishEvent method
        self.guid = odcportguid # So that when we call an odc method, we can work out which odcport to hand it too. 
        self.Enabled= False;
        self.LogDebug("SimPortClass Init Called - {}".format(objectname))        # No forward declaration in Python

    def Config(self, MainJSON, OverrideJSON):
        """ The JSON values should be passed as strings, which we then load into a dictionary for processing"""
        self.LogDebug("Passed Main JSON Config information - Len {} , {}".format(len(OverrideJSON),MainJSON))
        self.LogDebug("Passed Override JSON Config information - Len {} , {}".format(len(OverrideJSON), OverrideJSON))

        # Load JSON into Dicts
        Main = {}
        Override = {}
        try:
            if len(MainJSON) != 0:
                Main = json.loads(MainJSON)   
            if len(OverrideJSON) != 0:
                Override = json.loads(OverrideJSON)
        except:
           self.LogError("Exception on parsing JSON Config data - {}".format(sys.exc_info()[0]))
           return
        self.LogDebug("JSON Config strings Parsed")

    def Enable(self):
        self.LogTrace("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True;

    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    def EventHandler(self,EventType, Index, Time, Quality, Sender):
        self.LogDebug("EventHander: {}, {}".format(Sender, self.guid))
        return True
