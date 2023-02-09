import types
import json
import sys
from datetime import datetime, timedelta
import odc
from random import randint

# Logging Levels
Trace = 0
Debug = 1
Info = 2
Warn = 3
Error = 4
Critical = 5

# Exact string values for Event parameters which are passed as strings
# EventTypes, ConnectState,Binary,Analog,Counter,FrozenCounter,BinaryOutputStatus,AnalogOutputStatus,ControlRelayOutputBlock and others...
# QualityFlags, ONLINE,RESTART,COMM_LOST,REMOTE_FORCED,LOCAL_FORCE,OVERRANGE,REFERENCE_ERR,ROLLOVER,DISCONTINUITY,CHATTER_FILTER
# ConnectState, PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN
# ControlCode, NUL,NUL_CANCEL,PULSE_ON,PULSE_ON_CANCEL,PULSE_OFF,PULSE_OFF_CANCEL,LATCH_ON,LATCH_ON_CANCEL,LATCH_OFF,LATCH_OFF_CANCEL,
#               CLOSE_PULSE_ON,CLOSE_PULSE_ON_CANCEL,TRIP_PULSE_ON,TRIP_PULSE_ON_CANCEL,UNDEFINED

class SimPortClass:
    ''' Our class to handle an ODC Port. We must have __init__, ProcessJSONConfig, Enable, Disable, EventHander, TimerHandler and
    RestRequestHandler defined, as they will be called by our c/c++ code.
    ODC publishes some functions to this Module (when run) they are part of the odc module(include).
    We currently have odc.log, odc.SetTimer and odc.PublishEvent.
    '''

    # Worker Methods. They need to be high in the code so they are available in the code below. No forward declaration in Python
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


    # Required Method
    def __init__(self, odcportguid, objectname):
        self.objectname = objectname    # Documentation/error use only.
        self.guid = odcportguid         # So that when we call an odc method, ODC can work out which pyport to hand it too.
        self.Enabled = False
        self.ConfigDict = {}      # Config Dictionary
        self.LogDebug("***** SimPortClass Init Called - File Version 1 - {} *****".format(objectname))
        self.value = "0"
        self.binary = 0
        return

    # Required Method
    def Config(self, MainJSON, OverrideJSON):
        """ The JSON values are passed as strings (stripped of comments), which we then load into a dictionary for processing
        Note that this does not handle Inherits JSON entries correctly (Inherits is effectily an Include file entry)"""
        #self.LogDebug("Passed Main JSON Config information - Len {} , {}".format(len(MainJSON),MainJSON))
        #self.LogDebug("Passed Override JSON Config information - Len {} , {}".format(len(OverrideJSON), OverrideJSON))

        # Load JSON into Dicts
        Override = {}
        try:
            if len(MainJSON) != 0:
                self.ConfigDict = json.loads(MainJSON)
            if len(OverrideJSON) != 0:
                Override = json.loads(OverrideJSON)
        except:
           self.LogError("Exception on parsing JSON Config data - {}".format(sys.exc_info()[0]))
           return

        self.LogDebug("JSON Config strings Parsed")

        # Now use the override config settings to adjust or add to the MainConfig. Only root json values can be adjusted.
        # So you cannot change a single value in a Binary point definition without rewriting the whole "Binaries" json key.
        self.ConfigDict.update(Override)               # Merges with Override doing just that - no recursion into sub dictionaries

        self.LogDebug("Combined (Merged) JSON Config {}".format(json.dumps(self.ConfigDict)))

        # Now extract what is needed for this instance, or just reference the ConfigDict when needed.
        return

    # Required Method
    def Operational(self):
        """ This is called from ODC once ODC is ready for us to be fully operational - normally after Build is complete"""
        self.LogDebug("Port Operational - {}".format(datetime.now().isoformat(" ")))

        # send first binary high after 10sec
        odc.SetTimer(self.guid, 1, 10000)    # in msec
        return

    # Required Method
    def Enable(self):
        self.LogTrace("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True
        return

    # Required Method
    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        self.LogTrace("EventHander: {}, {}, {} {} - {}".format(self.guid,Sender,Index,EventType,Payload))

        #odc.PublishEvent(self.guid,EventType,Index,Quality,Payload)  # Echoing Event for testing. Sender, Time auto created in ODC
        return True

    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout,
    # so you can have multiple timers running.
    def TimerHandler(self,TimerId):
        self.LogTrace("TimerHander: ID {}, {}".format(TimerId, self.guid))

        # this code sets a binary 0 high for a random time period between 1-10 minutes (timer_2)
        # it then sleeps for a random time period between 1-120 minutes (timer_1) and changes the binary index to 1
        # binary 1 will then go high for for a random time period between 1-10 minutes (timer_2)
        # it then sleeps for a random time period between 1-120 minutes (timer_1) and changes the binary index back to 0
        # repeat...

        if (TimerId == 1):
            self.value = "1"
            odc.PublishEvent(self.guid,"Binary", self.binary,"|ONLINE|", self.value)
            self.LogDebug("Binary {} set to {}".format(self.binary, self.value))
            # set timer 2 to send the binary low sometime int he next 10min
            timer_2 = randint(1,10)*60000
            odc.SetTimer(self.guid, 2, timer_2)
            next = datetime.now() + timedelta(minutes=timer_2/60000)
            self.LogDebug("Binary {} will go low at {}".format(self.binary, next.isoformat(" ")))
            
        elif (TimerId == 2):
            self.value = "0"
            odc.PublishEvent(self.guid,"Binary", self.binary,"|ONLINE|", self.value)
            self.LogDebug("Binary {} set to {}".format(self.binary, self.value))
            # set timer 1 to send the binary high again in the next 120min
            timer_1 = randint(1,120)*60000
            odc.SetTimer(self.guid, 1, timer_1)
            if self.binary:
                self.binary = 0
            else:
                self.binary = 1
            next = datetime.now() + timedelta(minutes=timer_1/60000)
            self.LogDebug("Binary {} will go high at {}".format(self.binary, next.isoformat(" ")))

        return

    # The Rest response interface - the following method will be called whenever the restful interface (a single interface for all PythonPorts) gets
    # called. It will be decoded sufficiently so that it is passed to the correct PythonPort (us)
    # To make these calls in Python (our test scripts) we can use the library below.
    # https://2.python-requests.org//en/master/
    #
    # We return the response that we want sent back to the caller. This will be a JSON string. A null string would be an error.
    def RestRequestHandler(self, url, content):
        self.LogTrace("RestRequestHander: {}".format(url))

        return 