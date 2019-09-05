import types
import json
import sys
import time
import math
from datetime import datetime, timezone
import odc
# getting a problem loading Kafka module - [No module named 'confluent_kafka.cimpl']
# This is due the .cimpl file being installed into the release library .zip file, so is not fund under debug builds.
# Could just copy the C:\Python37\python37.zip to C:\Python37\python37_d.zip, or just use release ODC.
# Also could get this error if running an "embedded" python ODC build. The library has to be iinstalled into that embedded python.
# Not sure how to do that yet.
from confluent_kafka import Producer, Consumer, KafkaError

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

    # Mandatory Methods that are called by ODC PyPort

    def __init__(self, odcportguid, objectname):
        self.objectname = objectname    # Documentation/error use only.
        self.guid = odcportguid         # So that when we call an odc method, ODC can work out which pyport to hand it too. 
        self.Enabled = False;
        self.MessageIndex = 0
        self.LastMessageIndex = self.MessageIndex
        self.StartTimeSeconds = time.time()
        self.ConfigDict = {}      # Config Dictionary
        self.LogDebug("PyPortKafka - SimPortClass Init Called - {}".format(objectname)) 
        self.LogDebug("Python sys.path - {}".format(sys.path))         
        return

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
        kafkaserver = "{}:{}".format(self.ConfigDict["BrokerIP"],self.ConfigDict["BrokerPort"])

        # The acks can be 0, 1, 2 etc or all. It is the number of nodes that have to have written the message before we get acknoledgement.
        # So a value of 0 is fire and forget. etc.
        # Details of what can be passed see: https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
        #
        # Really interesting discussion of how to loose messages in Kafka (but also how not to loose messages!)
        # https://jack-vanlightly.com/blog/2018/9/14/how-to-lose-messages-on-a-kafka-cluster-part1
        #
        conf = {'bootstrap.servers': kafkaserver, 'client.id': 'OpenDataCon', 'default.topic.config': {'acks': 'all'}}
        self.producer = Producer(conf)        
        return

    def Operational(self):
        """ This is called from ODC once ODC is ready for us to be fully operational - normally after Build is complete"""
        self.LogDebug("Port Operational - {}".format(datetime.now().isoformat(" ")))
        # This is only done once - will self restart from the timer callback.
        odc.SetTimer(self.guid, 1, 500)    # Start the timer cycle

        odc.SetTimer(self.guid, 2, 100)    # Start the timer cycle
        return

    def Enable(self):
        self.LogDebug("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True;
        return

    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return

    # Look for the matching PITag for this point. If we dont find it return False.
    # This is going to get called a lot, so may need to be optimised.
    def GetPITag(self, EventType, ODCIndex):
        
        # Perhaps split this on startup, so we dont do it every time.
        Json = self.ConfigDict.get(EventType,"")    # If no configured points for a point type, return empty Json

        if (len(Json)==0):
            self.LogError("Can't find point definitions for {} types".format(EventType))
            return ""

        #TODO Do we need a faster way of doing this? Potentially 10,000 points or more.
        for x in Json:
            if(x["Index"] == ODCIndex):                
                return x["PITag"]       # we have a match!

        return ""

    def delivery_report(self, err, msg):
        """ Called once for each message produced to indicate delivery result. Triggered by poll() or flush(). """
        if err is not None:
            self.LogError('Message delivery failed: {}'.format(err))
        #else:
        #    self.LogDebug('Message delivered to {} [{}]'.format(msg.topic(), msg.partition()))

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        # self.LogDebug("EventHander: {}, {}, {} {} - {}".format(self.guid,Sender,Index,EventType,Payload))

        if (EventType == "ConnectState"):
            return True

        PITag = self.GetPITag(EventType, Index)

        if (len(PITag)):
            # We have a PITag so send to Kafka
            # TimeDate Needs to be "2019-07-17T01:34:20.072Z" ISO8601 format.
            ISOTimeStamp = "{}.{:03}Z".format(datetime.fromtimestamp(Time/1000,timezone.utc).strftime("%FT%T"), Time%1000)
            messagevalue = {"PITag" : PITag, "Index" : self.MessageIndex, "Value" : Payload, "Quality" : Quality, "TimeStamp" : ISOTimeStamp}
            self.MessageIndex = self.MessageIndex + 1   # This is just so we can reconstruct message order after retreiving data from Kafka

            # Potentially check this call - we could run out of memory if nothing is going to Kafka.
            #TODO There is a maximum number of messages that can be set.
            try:
                self.producer.produce(self.ConfigDict["Topic"], value=json.dumps(messagevalue), callback=self.delivery_report)
             
            except BufferError:
                self.LogError("Kafka Producer Queue is full ({} messages awaiting delivery)".format(len(self.producer)))
            
        else:
            self.LogError("Could not find PITag for Point {}, {} - {}".format(Sender,EventType,Index))

        # Always return True - we processed the message - even if we could not pass it to Kafka.     
        return True

    def millisdiff(self, starttimedate):
        dt = datetime.now() - starttimedate
        ms = (dt.days * 24 * 60 * 60 + dt.seconds) * 1000 + dt.microseconds / 1000.0
        return ms

    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout, 
    # so you can have multiple timers running.
    def TimerHandler(self,TimerId):
        # self.LogDebug("TimerHander: ID {}, {}".format(TimerId, self.guid))

        self.producer.poll(0)   # Do any waiting processing, but dont wait!
        thresholdcount = 10000

        if (TimerId == 1):
            EventCount = 1
            starttime = datetime.now()

            # Get Events from the queue and process them, up until we have an empty queue or 100 entries OR
            # The local producer queue has 100 entries in it.
            # Then trigger the kafka library to send them.
            while ((EventCount < thresholdcount) and (len(self.producer) < thresholdcount)):
                EventCount += 1
                EventType, Index, Time, Quality, Payload, Sender = odc.GetNextEvent(self.guid)

                # The EventType will be an empty string if the queue is empty.
                if (len(EventType) == 0):
                    break

                # We already have the event method written to handle this so just use. 
                # This means the config file flag can swap between the two processes with no code changes
                self.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
                self.producer.poll(0)   # Do any waiting processing, but dont wait!

            self.LogDebug("Kafka Produced {} messages. Kafka queue size {}. Execution time {} msec".format(EventCount,len(self.producer),self.millisdiff(starttime)))

            # If we have pushed the maxiumum number of events in, we need to go faster...
            # If the producer queue hits the limit, this means the kafka cluster is not keeping up.
            if EventCount < thresholdcount:
                odc.SetTimer(self.guid, 1, 500)    # Make it so we fire again in .5 second.
            else:
                odc.SetTimer(self.guid, 1, 250)

        # Can use timer 2 to run poll() more frequently. Must be started in Operational()
        # Running poll more often does not seem to make a difference.
        if (TimerId == 2):
            odc.SetTimer(self.guid, 2, 100)

        return

    # The Rest response interface - the following method will be called whenever the restful interface (a single interface for all PythonPorts) gets
    # called. It will be decoded sufficiently so that it is passed to the correct PythonPort (us)
    # To make these calls in Python (our test scripts) we can use the library below.
    # https://2.python-requests.org//en/master/
    #
    # We return the response that we want sent back to the caller. This will be a JSON string. A null string would be an error.
    def RestRequestHandler(self, url, content):
        self.LogDebug("RestRequestHander: {}".format(url))
       
        Response = {}   # Empty Dict
        if ("GET" in url):
            DeltaSeconds = time.time() - self.StartTimeSeconds
            Response["Status"] = "PyPortKafka Processing is running. Messages Processed - {}, Messages/Second - {}".format(self.MessageIndex, math.floor((self.MessageIndex - self.LastMessageIndex)/DeltaSeconds))

            self.StartTimeSeconds = time.time()
            self.LastMessageIndex = self.MessageIndex

        return json.dumps(Response)