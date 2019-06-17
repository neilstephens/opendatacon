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

# QualityFlags,ONLINE,RESTART,COMM_LOST,REMOTE_FORCED,LOCAL_FORCE,OVERRANGE,REFERENCE_ERR,ROLLOVER,DISCONTINUITY,CHATTER_FILTER

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
        self.i = 0
        self.LogDebug("SimPortClass Init Called - {}".format(objectname))        # No forward declaration in Python
        return

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
        return

    def Enable(self):
        self.LogTrace("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True;
        return

    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string ) Sender (string)
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        self.LogDebug("EventHander: {}, {}, {} {} - {}".format(self.guid,Sender,Index,EventType,Payload))

        if (EventType == "Binary"):
            self.LogDebug("Event is a Binary")
        if ("ONLINE" not in Quality):
            self.LogDebug("Event Quality not ONLINE")
        
        odc.PublishEvent(self.guid,Sender,Index,EventType,Payload)  # Echoing Event for testing. Sender, Time auto created in ODC
        return True

    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout, 
    # so you can have multiple timers running.
    def TimerHandler(self,TimerId):
        self.LogDebug("TimerHander: ID {}, {}".format(TimerId, self.guid))
        return

    # The Rest response interface - the following method will be called whenever the restful interface (a single interface for all PythonPorts) gets
    # called. It will be decode sufficiently so that it is passed to the correct PythonPort (us)
    # To make these calls in Python (our test scripts) we can use the library below.
    # https://2.python-requests.org//en/master/
    #
    # We return the response that we want sent back to the caller. This will be a JSON string. A null string would be an error.
    def RestRequestHandler(self, url):
        self.LogDebug("RestRequestHander: {}".format(url))
        Response = {}   # Empty Dict
        Response['test'] = "Hello"

        odc.SetTimer(self.guid, self.i, 1001-self.i)    # Set a timer to go off in 1 second
        self.i = self.i + 1
        self.LogDebug("RestRequestHander: Sent Set Timer Command {}".format(self.i))

        return json.dumps(Response)