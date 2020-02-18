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
        self.EventQueueSize = 0;
        self.QueueErrorState = 0;       # 0 - No Error, 1 - Error, Notified,
        self.SendErrorState = 0;        # As above
        self.LastMessageIndex = self.MessageIndex
        self.StartTimeSeconds = time.time()
        self.timestart = 1.1 # Used for profiling, setup as a float
        self.measuretimeus = 0
        self.measuretimeus2 = 0
        self.ConfigDict = {}      # Config Dictionary
        self.LogDebug("PyPortKafka - SimPortClass Init Called - {}".format(objectname))
        self.LogDebug("Python sys.path - {}".format(sys.path))
        return

        # time.perf_counter_ns() (3.6 up) gives the time including include sleeps time.process_time() is overall time.
    def timeusstart(self):
         self.timestart = time.perf_counter() #float fractions of a second

    def timeusstop(self):
        return int((time.perf_counter()-self.timestart)*1000000)

    def minutetimermessage(self):
        DeltaSeconds = time.time() - self.StartTimeSeconds
        self.LogError("PyPortKafka status. Event Queue {}, Messages Processed - {}, Messages/Second - {}".format(self.EventQueueSize, self.MessageIndex, math.floor((self.MessageIndex - self.LastMessageIndex)/DeltaSeconds)))
        self.StartTimeSeconds = time.time()
        self.LastMessageIndex = self.MessageIndex
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
        kafkaserver = self.ConfigDict["bootstrap.servers"]
        self.topic = self.ConfigDict["Topic"]

        # The acks can be 0, 1, 2 etc or all. It is the number of nodes that have to have written the message before we get acknoledgement.
        # So a value of 0 is fire and forget. etc.
        # Details of what can be passed see: https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
        # We require all of the nodes that are configured to ack for the data to be valid - we dont control it here.
        # In the cluster config it is set to 2.
        #
        # Really interesting discussion of how to loose messages in Kafka (but also how not to loose messages!)
        # https://jack-vanlightly.com/blog/2018/9/14/how-to-lose-messages-on-a-kafka-cluster-part1
        #
        # To control batching - the values are the defaults: 'batch.num.messages': 10000 OR 'message.max.bytes' : 1000000,
        # queuing.strategy=fifo an attempt to make sure messages arrive in the order they are sent..
        # max.in.flight.requests.per.connection=5 Usually a very large number, 5 does not seem to slow things down - maybe 1%?
        # request.required.acks are the number of copies of the message pushed to the other nodes. Really dont need any acks, if the primary has the message, it will replicate shortly. Or could require an ack from at least 1 node
        # 'compression.type':'none' - does not seem to make much difference (none,snappy, gzip)
        # 'delivery.report.only.error':False    This does make things quicker from approx 70k/sec to 74k/sec
        # 'message.send.max.retries': 100000 - will not drop the message until this many attempts have been made...
        conf = {'bootstrap.servers': kafkaserver, 'client.id': 'OpenDataCon', 'delivery.report.only.error' : True, 'message.send.max.retries': 10000000, 'request.required.acks' : 0, 'max.in.flight.requests.per.connection' : 100}
        self.producer = Producer(conf)
        return

    def Operational(self):
        """ This is called from ODC once ODC is ready for us to be fully operational - normally after Build is complete"""
        self.LogDebug("Port Operational - {}".format(datetime.now().isoformat(" ")))
        # This is only done once - will self restart from the timer callback.
        odc.SetTimer(self.guid, 1, 500)    # Start the timer cycle
        odc.SetTimer(self.guid, 2, 10000)   # First status message after 10 seconds
        return

    def Enable(self):
        self.LogDebug("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True;
        return

    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return

    # Not used
    def delivery_report(self, err, msg):
        """ Called once for each message produced to indicate delivery result. Triggered by poll() or flush(). """
        if err is not None:
            if self.SendErrorState == 0:
                self.LogError("Kafka Send Message Error - {} [{}] - {}".format(msg.topic(), msg.partition(), err))
                self.SendErrorState = 1
        else:
            if self.SendErrorState == 1:
                self.LogError("Kafka Send Message Error Cleared - {} [{}]".format(msg.topic(), msg.partition()))
                self.SendErrorState = 0

    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        # self.LogDebug("EventHander: {}, {}, {} {} - {}".format(self.guid,Sender,Index,EventType,Payload))

        if (EventType == "ConnectState"):
            return True

        self.LogError("Events must be queued {}".format(EventType))

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

        if (TimerId == 1):

            MaxMessageCount = 5000
            longwaitmsec = 100
            shortwaitmsec = 5
            EventCount = 1
            starttime = datetime.now()
            self.measuretimeus = 0
            self.measuretimeus2 = 0

            # Get Events from the queue and process them, up until we have an empty queue or MaxMessageCount entries
            # Then trigger the Kafka library to send them.

            while ((EventCount < MaxMessageCount)):
                EventCount += 1

                self.timeusstart()
                ### Takes about 3.2usec (old 8.4usec) per call (approx) on DEV server
                JsonEventstr, empty = odc.GetNextEvent(self.guid)
                self.measuretimeus += self.timeusstop()

                # The EventType will be an empty string if the queue is empty.
                if (empty == True):
                    break

                try:
                    self.timeusstart()
                    # Now 32msec/5000, about 5usec per record. (old 45msec/5000 so 9usec/record)
                    # Can we only get a single delivery report per block of up to 5000 messages?
                    # If we set the re-try count to max int, then handling the delivery report does not make much sense - the buffer will just fill up and then we will get an exception
                    # here due to a full buffer. And we fill up the next buffer (in PyPort) (note we need to store the event we were about to send so we dont loose it!)
                    # Eventually we will loose events, but there is nothing we can do about that.
                    self.producer.produce(self.topic, value=JsonEventstr)
                    self.measuretimeus2 += self.timeusstop()
                    if self.QueueErrorState == 1:
                        self.LogError("Kafka Producer Queue Recovered - NOT full ({} messages awaiting delivery)".format(len(self.producer)))
                        self.QueueErrorState = 0

                except BufferError:
                    if self.QueueErrorState == 0:
                        self.LogError("Kafka Producer Queue is full ({} messages awaiting delivery)".format(len(self.producer)))
                        self.QueueErrorState = 1
                    break

                if (EventCount % 100 == 0):
                    self.producer.poll(0)   # Do any waiting processing, but dont wait!

            self.EventQueueSize = odc.GetEventQueueSize(self.guid);
            #self.LogDebug("Kafka Produced {} messages. Kafka queue size {}. ODC Event queue size {} Execution time {} msec Timed code {}, {} us".format(EventCount,len(self.producer),self.EventQueueSize,self.millisdiff(starttime),self.measuretimeus,self.measuretimeus2))

            self.MessageIndex += EventCount

            # If we have pushed the maximum number of events in, we need to go faster...
            # If the producer queue hits the limit, this means the kafka cluster is not keeping up.
            if EventCount < MaxMessageCount:
                odc.SetTimer(self.guid, 1, longwaitmsec)    # We do not have messages waiting...
            else:
                odc.SetTimer(self.guid, 1, shortwaitmsec)   # We do have messages waiting

        if (TimerId == 2):
            self.minutetimermessage()
            odc.SetTimer(self.guid, 2, 10000)   # Set to run again in a minute

        self.producer.poll(0)   # Do any waiting processing, but dont wait!

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