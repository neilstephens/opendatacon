import types
import json
import sys
from datetime import datetime
import odc
from urllib.parse import urlparse, parse_qs

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

#
# This is a Circuit Breaker Simulation code
#
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
        self.Enabled = False;
        self.i = 0
        self.ConfigDict = {}      # Config Dictionary
        self.LogDebug("SimPortClass Init Called - {}".format(objectname))       
        return

    # Required Method
    def Config(self, MainJSON, OverrideJSON):
        """ The JSON values are passed as strings (stripped of comments), which we then load into a dictionary for processing
        Note that this does not handle Inherits JSON entries correctly (Inherits is effectily an Include file entry)"""
        self.LogDebug("Passed Main JSON Config information - Len {} , {}".format(len(MainJSON),MainJSON))
        self.LogDebug("Passed Override JSON Config information - Len {} , {}".format(len(OverrideJSON), OverrideJSON))

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
    def Enable(self):
        self.LogTrace("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True;
        return

    # Required Method
    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return

    #-------------------Worker Methods------------------
    # Get the current state for a digital bit, matching by the passed parameters.
    def GetState(self, Json, CBNumber, CBStateBit):
        for x in Json:
            if(x["CBNumber"] == CBNumber) and (x["SimType"] == CBStateBit):
                # we have a match!
                return x["State"]
        raise Exception("Could not find a matching State field for {}, {}".format(CBNumber, CBStateBit))

    # Get the current state for a digital bit, matching by the passed parameters.
    def GetStateFromIndex(self, Json, Index):
        for x in Json:
            if(x["Index"] == Index):
                # we have a match!
                return x["State"]
        raise Exception("Could not find a matching State field for Index {}}".format(Index))

    # Return a string indicating what the two binary bits mean
    def GetCombinedState(self,Bit1, Bit0):
        CBState = [["FaultZeros","Closed"],["Open","FaultOnes"]]
        return CBState[Bit1][Bit0]

    def SetState(self, Json, CBNumber, CBStateBit, State):
        for x in Json:
            if(x["CBNumber"] == CBNumber) and (x["SimType"] == CBStateBit):
                # we have a match! 

                if (x["State"] == State): 
                    self.LogDebug("No Change in state bit {}".format(CBStateBit))
                    return # nothing to do, the bit has not changed

                x["State"] = State
                odc.PublishEvent(self.guid,"Binary",x["Index"],"|ONLINE|",str(State))  # Need to send this change through as an event 
                return

        raise Exception("Could not find a matching State field for {}, {}".format(CBNumber, CBStateBit))

    def SetCombinedState(self, Json, CBNumber, Command):
        CBState = {"FaultZeros":[0,0],
                   "Closed":[1,0],
                   "Open":[0,1],
                   "FaultOnes":[1,1],
                   "Fault":[1,1]}
        StateBits = CBState[Command]
        self.LogDebug("Setting CBState of {} to {}".format(CBNumber,Command))

        self.SetState( Json, CBNumber, "CBStateBit0", StateBits[0])
        self.SetState( Json, CBNumber, "CBStateBit1", StateBits[1])

    def GetControlDetailsFromIndex(self, Json, Index):
        for x in Json:
            if(x["Index"] == Index):
                # we have a match!
                return x["CBNumber"], x["CBCommand"]
        raise Exception("Could not find a matching CBNumber and/or CBCommand field for Index {}}".format(Index))

    #---------------- Response Methods --------------------

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    # Required Method
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        self.LogDebug("EventHander: {}, {}, {} {} {} - {}".format(self.guid,Sender,Index,EventType,Quality,Payload))

        if (EventType == "ControlRelayOutputBlock"):
            # Need to work out which Circuit Breaker and what command
            CBNumber, CBCommand = self.GetControlDetailsFromIndex(self.ConfigDict["BinaryControls"], Index)
            if (CBCommand == "Trip"):
                self.SetCombinedState(self.ConfigDict["Binaries"], CBNumber, "Open")
            elif(CBCommand == "Close"):
                self.SetCombinedState(self.ConfigDict["Binaries"], CBNumber, "Closed")
            else:
                self.LogDebug("Command received not recognised - {}".format(CBCommand))

        elif (EventType == "ConnectState"):
            self.LogDebug("ConnectState Event {}".format(Payload))  #PORT_UP/CONNECTED/DISCONNECTED/PORT_DOWN
            #TODO Do we need to take action on ConnectState Events?

        else:
            self.LogDebug("Event is not one we are interested in - Ignoring")

        #if ("ONLINE" not in Quality):
        #    self.LogDebug("Event Quality not ONLINE")
        
        # Any changes that were made to the state, triggered events when they were made.        
        return True

    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout, 
    # so you can have multiple timers running.
    # Required Method
    def TimerHandler(self,TimerId):
        self.LogDebug("TimerHander: ID {}, {}".format(TimerId, self.guid))
        return  

    # The Rest response interface - the following method will be called whenever the restful interface (a single interface for all PythonPorts) gets
    # called. It will be decoded sufficiently so that it is passed to the correct PythonPort (us)
    #
    # We return the response that we want sent back to the caller. This will be a JSON string for GET. A null (empty) string will be reported as a bad request by the c++ code.
    # Required Method
    def RestRequestHandler(self, eurl, content):

        Response = {}   # Empty Dict
        url = ""
        try:
            HttpMethod = ""
            if ("GET" in eurl):
                url = eurl.replace("GET ","",1)
                HttpMethod = "GET"
            elif ("POST" in eurl):
                url = eurl.replace("POST ","",1)
                HttpMethod = "POST"
            else:
                self.LogError("PyPort only supports GET and POST Http Requests")
                return ""   # Will cause an error response to the request

            urlp = urlparse(url)    # Split into sections that we can reference. Mainly to get parameters            
            
            # We have only one get option at the moment - Status with CBNumber as the parameter.
            if (HttpMethod == "GET" and "/PyPortCBSim/status" in eurl):
                self.LogDebug("GET url {} ".format(url))
                qs = parse_qs(urlp.query)
                CBNumber = int(qs["CBNumber"][0])
                Response["CBStateBit0"] = self.GetState(self.ConfigDict["Binaries"], CBNumber, "CBStateBit0")
                Response["CBStateBit1"] = self.GetState(self.ConfigDict["Binaries"], CBNumber, "CBStateBit1")
                Response["State"] = self.GetCombinedState(Response["CBStateBit1"],Response["CBStateBit0"])

            elif(HttpMethod == "POST" and "/PyPortCBSim/set" in eurl):
                # Any failure in decoding the content will trigger an exception, and an error to the PUSH request
                self.LogDebug("POST url {} content {}".format(url, content))

                """{"CBNumber" : 1, "CBState" : "Open"/"Closed"/"FaultZeros"/"FaultOnes"/"Fault" } """
                jPayload = {}
                jPayload = json.loads(content)
                
                self.SetCombinedState(self.ConfigDict["Binaries"], jPayload["CBNumber"], jPayload["CBState"])

                Response["Result"] = "OK"

                # Any changes that were made to the state, triggered events when they were made. 

            else:
                self.LogError("Illegal http request")
                return ""

        except (RuntimeError, TypeError, NameError, Exception) as e:
            print("Exception - {}".format(e))
            return ""

        return json.dumps(Response)

    