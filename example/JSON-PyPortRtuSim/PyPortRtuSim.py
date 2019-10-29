import types
import json
import sys
import re
import numpy
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
# This is a Simulator for an RTU and the equipment connected to it.
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
        self.LogDebug("PyPortRtuSim - SimPortClass Init Called - {}".format(objectname))
        self.LogDebug("Python sys.path - {}".format(sys.path))
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

    # Worker
    def SendInitialState(self):
        if "Analogs" in self.ConfigDict:
            for x in self.ConfigDict["Analogs"]:
               odc.PublishEvent(self.guid,"Analog",x["Index"],"|ONLINE|",str(x["Value"]))

        if "Binaries" in self.ConfigDict:
            for x in self.ConfigDict["Binaries"]:
                   odc.PublishEvent(self.guid,"Binary",x["Index"],"|ONLINE|",str(x["Value"]))
        return

    #-------------------Worker Methods------------------
    # Get the current state for a digital bit, matching by the passed parameters.
    def GetBinaryValue(self, Type, Number, BitID):
        for x in self.ConfigDict["Binaries"]:
            if (x["Type"] == Type) and (x["Number"] == Number) and (x["BitID"] == BitID):
                # we have a match!
                return x["Value"]
        raise Exception("Could not find a matching binary value for {}, {}, {}".format(Type, Number, BitID))

    # Get the current state for a digital bit, matching by the passed parameters.
    def GetBinaryValueUsingIndex(self, Index):
        for x in self.ConfigDict["Binaries"]:
            if (x["Index"] == Index):
                # we have a match!
                return x["Value"]
        raise Exception("Could not find a matching binary value for Index {}".format(Index))

    def SetBinaryValue(self, Type, Number, BitID, Value):
        for x in self.ConfigDict["Binaries"]:
            if (x["Type"] == Type) and (x["Number"] == Number) and (x["BitID"] == BitID):

                if type(Value) is str:  # Handle string values
                    Value = int(Value)

                if (x["Value"] == Value):
                    self.LogDebug("No Change in value of bit {}".format(BitID))
                    return # nothing to do, the bit has not changed

                x["Value"] = Value
                odc.PublishEvent(self.guid,"Binary",x["Index"],"|ONLINE|",str(Value))  # Need to send this change through as an event
                return

        raise Exception("Could not find a matching binary value for {}, {}, {}".format(Type, Number, BitID))

    def SetBinaryValueUsingIndex(self, Index, Value):
        for x in self.ConfigDict["Binaries"]:
            if (x["Index"] == Index):
                # we have a match!
                if type(Value) is str:  # Handle string values
                    Value = int(Value)

                if (x["Value"] == Value):
                    self.LogDebug("No Change in value of bit {}".format(BitID))
                    return # nothing to do, the bit has not changed

                x["Value"] = Value
                odc.PublishEvent(self.guid,"Binary",x["Index"],"|ONLINE|",str(Value))  # Need to send this change through as an event
                return

        raise Exception("Could not find a matching binary value for Index {}".format(Index))

    def GetAnalogValue(self, Type, Number):
        for x in self.ConfigDict["Analogs"]:
            if (x["Type"] == Type) and (Type == "TapChanger"):
                if x["Number"] == Number:
                    return x["Value"]

            elif (x["Type"] == Type) and (Type == "Sim"):
                if x["Index"] == Number:    # No number for Sim types, revert to ODCIndex
                    return x["Value"]

        raise Exception("Could not find a matching analog field for {}, {}".format(Type, Number))

    # Get the current state for a digital bit, matching by the passed parameters.
    def GetAnalogValueUsingIndex(self, Index):
        for x in self.ConfigDict["Analogs"]:
            if (x["Index"] == Index):
                # we have a match!
                return x["Value"]
        raise Exception("Could not find a matching analog field for Index {}".format(Index))

    def SetAnalogValue(self, Type, Number, Value):
        for x in self.ConfigDict["Analogs"]:
            if (x["Type"] == Type) and (Type == "Sim"):
                if x["Index"] == Number:    # No number for Sim types, revert to ODCIndex
                    if type(Value) is str:  # Handle string values
                        Value = int(Value)
                    x["Value"] = Value
                    odc.PublishEvent(self.guid,"Analog",x["Index"],"|ONLINE|",str(Value))  # Need to send this change through as an event
                    return

            elif (x["Type"] == Type) and (x["Number"] == Number):
                if type(Value) is str:  # Handle string values
                    Value = int(Value)
                x["Value"] = Value
                odc.PublishEvent(self.guid,"Analog",x["Index"],"|ONLINE|",str(Value))  # Need to send this change through as an event
                return

        raise Exception("Could not find a matching analog field for {}, {}".format(Type, Number))

    def SetAnalogValueUsingIndex(self, Index, Value):
        for x in self.ConfigDict["Analogs"]:
            if (x["Index"] == Index):

                if type(Value) is str:  # Handle string values
                    Value = int(Value)

                if (x["Value"] == Value):
                    self.LogDebug("No Change in value of analog {}".format(Index))
                    return # nothing to do, the value has not changed

                x["Value"] = Value
                odc.PublishEvent(self.guid,"Analog",x["Index"],"|ONLINE|",str(Value))  # Need to send this change through as an event
                return

        raise Exception("Could not find a matching Value field for {}".format(Index))

    def UpdateAnalogValue(self, x, secs):
        # use the secs, value, mean and std dev to calculate a new value - also if there is actually a new value.
        try:
            newval = numpy.random.normal(x["Mean"], x["StdDev"])
            if (newval != x["Value"]):
                x["Value"] = newval
                odc.PublishEvent(self.guid,"Analog",x["Index"],"|ONLINE|",str(x["Value"]))  # Need to send this change through as an event

        except (RuntimeError, TypeError, NameError, Exception) as e:
            self.LogError("Exception - {}".format(e))
            return  # dont change anything
        return

    def UpdateAnalogSimValues(self,secs):
        for x in self.ConfigDict["Analogs"]:
            if (x["Type"] == "Sim"):
                if "Timer" in x:
                    x["Timer"] -= secs*1000;
                    if (x["Timer"] <= 0):
                        #Timer expired! Send Event
                        x["Timer"] = x[ "UpdateRate"];

                        self.UpdateAnalogValue(x,secs)

                else:
                    if ("UpdateRate" in x):
                        if (x["UpdateRate"] != 0):   # If 0 or not a field, do not do updates.
                            # Need to randomise the start count down period so they dont fire all at once.
                            x["Timer"] = numpy.random.randint(0,x["UpdateRate"])
        return

    # Return a string indicating what the two binary bits mean
    def CBGetCombinedState(self,Bit1, Bit0):
        CBState = [["Maintenance","Closed"],["Open","Fault"]]
        return CBState[Bit1][Bit0]

    def CBSetState(self, Number, Command):
        CBState = {"MaintenanceZeros":[0,0],
                   "Maintenance":[0,0],
                   "Closed":[1,0],
                   "Open":[0,1],
                   "FaultOnes":[1,1],
                   "Fault":[1,1]}
        StateBits = CBState[Command]
        self.LogDebug("Setting CBState of {} to {}".format(Number,Command))

        self.SetBinaryValue( "CB", Number, 0, StateBits[0])
        self.SetBinaryValue( "CB", Number, 1, StateBits[1])
        return

    def GetFBType( self, Number ):
        for x in self.ConfigDict["BinaryControls"]:
            if (x["Type"] == "TapChanger") and (x["Number"] == Number):
               return x["FB"]
        return "Error"

    def GetBCDValue(self, Number):
        val = 0
        for x in self.ConfigDict["Binaries"]:
            if (x["Type"]=="TapChanger") and (x["Number"]==Number):
                val += x["Value"] << x["BitID"]
        return val

    def SetBCDValue(self, Number,val):
        for x in self.ConfigDict["Binaries"]:
            if (x["Type"]=="TapChanger") and (x["Number"]==Number):
                x["Value"] = 1 & (val >> x["BitID"])
        return

    def GetTapChangerValue( self, Number, FBType = None):
        if not FBType:
            FBType = self.GetFBType( Number )

        if (FBType == "ANA"):
            return self.GetAnalogValue("TapChanger",Number)

        if (FBType == "BCD"):
            return self.GetBCDValue(Number)
        self.LogError("Illegal Type passed to GetTapChangerValue - {}".format(FBType))
        return

    def SetTapChangerValue( self, Number, val, FBType = None):
        if not FBType:
            FBType = self.GetFBType( Number )

        if (FBType == "ANA"):
            self.SetAnalogValue("TapChanger",Number,val)
            return

        if (FBType == "BCD"):
            self.SetBCDValue(Number,val)
            return

        self.LogError("Illegal Feedback Type passed to SetTapChangerValue - {}".format(FBType))
        return

    def TapChangerTapUp( self, Number, FBType, Max ):
        # Does the tapup command and limits the value to the maximum allowed.
        self.LogDebug("TapUpCommand")
        val = self.GetTapChangerValue(Number,FBType)
        val += 1
        if (val <= Max):
            self.SetTapChangerValue(Number,val,FBType)
        return

    def TapChangerTapDown( self, Number, Type, Min ):
        # Does the tapdown command and limits the value to the minimum allowed.
        self.LogDebug("TapDownCommand")
        val = self.GetTapChangerValue(Number,FBType)
        val -= 1
        if (val >= Min):
            self.SetTapChangerValue(Number,val,FBType)
        return

    # Returns values from loaded config.  "Type" : "CB", "Number" : 1, "Command":"Trip"
    def GetControlDetailsUsingIndex(self, Index):
        for x in self.ConfigDict["BinaryControls"]:
            if(x["Index"] == Index):
                # we have a match!
                return x
        raise Exception("Could not find a matching Number and/or Command field for Index {}}".format(Index))

    #---------------- Required Methods --------------------
    def Operational(self):
        """ This is called from ODC once ODC is ready for us to be fully operational - normally after Build is complete"""
        self.LogTrace("Port Operational - {}".format(datetime.now().isoformat(" ")))

        self.SendInitialState()

        #odc.SetTimer(self.guid, 1, 60*1000)    # Set up to run for first time after 1 minute
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

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    # Required Method
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        self.LogDebug("EventHander: {}, {}, {} {} {} - {}".format(self.guid,Sender,Index,EventType,Quality,Payload))

        if (EventType == "ConnectState"):
            self.LogDebug("ConnectState Event {}".format(Payload))  #PORT_UP/CONNECTED/DISCONNECTED/PORT_DOWN
            if (Payload == "CONNECTED") :
                self.SendInitialState()
            return True

        if not re.search("ONLINE",Quality):
            self.LogDebug("Quality not ONLINE, ignoring event")
            return True;

        if (EventType == "ControlRelayOutputBlock"):
            # Need to work out which Circuit Breaker and what command

            x = self.GetControlDetailsUsingIndex(Index)
            if x["Type"] == "CB":
                # {"Index": 0, "Type" : "CB", "Number" : 1, "Command":"Trip"},
                if x["Command"] == "Trip":
                    self.CBSetState( x["Number"], "Open")
                elif x["Command"] == "Close":
                    self.CBSetState( x["Number"], "Closed")
                else:
                    self.LogDebug("Command received not recognised - {}".format(x["Command"]))

            elif x["Type"] == "TapChanger":
                # {"Index": 2, "Type" : "TapChanger", "Number" : 1, "FB": "ANA", "Max": 32, "Command":"TapUp"},
                self.LogDebug("Tap Changer")
                if x["Command"] == "TapUp":
                    self.TapChangerTapUp( x["Number"], x["FB"], x["Max"] )
                elif x["Command"] == "TapDown":
                    self.TapChangerTapDown( x["Number"], x["FB"], x["Min"] )
                else:
                    self.LogDebug("Command received not recognised - {}".format(x["Command"]))
            else:
                self.LogDebug("Command type received not recognised - {}".format(x["Type"]))

        elif (EventType == "Analog"):
            self.SetAnalogValueUsingIndex(Index,Payload)

        elif (EventType == "Binary"):
            self.SetBinaryValueUsingIndex(Index,Payload)

        else:
            self.LogDebug("Event is not one we are interested in - Ignoring")

        # Any changes that were made to the state, triggered events when they were made.
        return True

    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout,
    # so you can have multiple timers running.
    # Required Method
    def TimerHandler(self,TimerId):
        self.LogDebug("TimerHander: ID {}, {}".format(TimerId, self.guid))
        # This will be used to send Sim value changes that are the result of mean and std deviation values at the UpdateRate
        # Run once every 10 seconds?
        if (TimerId == 1):
            self.UpdateAnalogSimValues(10)  # Pass in seconds since last run
            odc.SetTimer(self.guid, 1, 10*1000)    # Set up to run again

        return

    # The Rest response interface - the following method will be called whenever the restful interface
    # (a single interface for all PythonPorts) gets called.
    # It will be decoded sufficiently so that it is passed to the correct PythonPort (us)
    #
    # We return the response that we want sent back to the caller. This will be a JSON string for GET.
    # A null (empty) string will be reported as a bad request by the c++ code.
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

            # We have only one get option at the moment - Status with Number as the parameter.
            if (HttpMethod == "GET" and "/PyPortRtuSim/status" in eurl):
                self.LogDebug("GET url {} ".format(url))
                qs = parse_qs(urlp.query)
                Number = int(qs["Number"][0])
                Type = qs["Type"][0]

                if Type == "CB":
                    self.LogDebug("Circuit Breaker Status Request - {}".format(Number))
                    Response["Bit0"] = self.GetBinaryValue( "CB", Number, 0)
                    Response["Bit1"] = self.GetBinaryValue( "CB", Number, 1)
                    Response["State"] = self.CBGetCombinedState(Response["Bit1"],Response["Bit0"])
                    return json.dumps(Response)

                if Type == "TapChanger":
                    self.LogDebug("Tap Changer Status Request - {}".format(Number))
                    Response["State"] = self.GetTapChangerValue(Number)
                    return json.dumps(Response)

                if Type == "Analog":
                    Response["State"] = self.GetAnalogValueUsingIndex(Number)
                    return json.dumps(Response)

                if Type == "Binary":
                    Response["State"] = self.GetBinaryValueUsingIndex(Number)
                    return json.dumps(Response)

            elif(HttpMethod == "POST" and "/PyPortRtuSim/set" in eurl):
                # Any failure in decoding the content will trigger an exception, and an error to the PUSH request
                self.LogDebug("POST url {} content {}".format(url, content))

                jPayload = {}
                jPayload = json.loads(content)

                if type(jPayload["Number"]) is str:
                    Number = int(jPayload["Number"])
                else:
                    Number = jPayload["Number"]
                Type = jPayload["Type"]

                """{"Type" : "CB", "Number" : 1, "State" : "Open"/"Closed"/"MaintenanceZeros"/"Maintenance"/"FaultOnes"/"Fault" } """
                if Type == "CB":
                    self.CBSetState( Number, jPayload["State"])
                    Response["Result"] = "OK"
                    return json.dumps(Response)

                if Type == "TapChanger":
                    self.SetTapChangerValue(Number,jPayload["State"])
                    Response["Result"] = "OK"
                    return json.dumps(Response)

                if Type == "Analog":
                    self.SetAnalogValueUsingIndex( Number, jPayload["State"])
                    Response["Result"] = "OK"
                    return json.dumps(Response)

                if Type == "Binary":
                    self.SetBinaryValueUsingIndex( Number, jPayload["State"])
                    Response["Result"] = "OK"
                    return json.dumps(Response)

            else:
                self.LogError("Illegal http request")

        except (RuntimeError, TypeError, NameError, Exception) as e:
            self.LogError("Exception - {}".format(e))
            Response["Result"] = "Exception - {}".format(e)
            return ""

        return ""