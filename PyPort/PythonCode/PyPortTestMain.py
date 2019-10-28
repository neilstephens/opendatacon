# This is a test scaffold for the PyPort Python template code.
# We create scaffolding so that we can build and run the Python code outside of
# ODC PyPort.
# Allows us to make sure everything works before we load it into ODC

# To support this we need an odc module to import (that simulates what odc
# offers), and a main function to
# create the Python class (like SimPortClass) and then call the methods with
# suitable test arguments
import odc
import PyPortSim
import PyPortRtuSim
import PyPortKafka
import requests
import json
# There is also a kafka-python library that can be used as well.  Different
# syntax though.
from confluent_kafka import Producer, Consumer, KafkaError

print("Staring Run")

conf = """{
            "IP" : "localhost",
            "Port" : 10000,

            "ModuleName" : "PyPortRtuSim",
            "ClassName": "SimPortClass",

            "Analogs" :
            [
                {"Index": 0, "Type" : "Sim", "Mean" : 500, "StdDev" : 1.2, "UpdateRate" : 60000, "Value": 1024 },
                {"Index": 1, "Type" : "TapChanger", "Number" : 1, "Value": 6}
            ]
            ,
            "Binaries" :
            [
                {"Index": 0, "Type" : "CB", "Number" : 1, "BitID" : 0, "Value": 0},
                {"Index": 1, "Type" : "CB", "Number" : 1, "BitID" : 1, "Value": 1},

                {"Index": 2, "Type" : "TapChanger", "Number" : 2, "BitID" : 0, "Value": 0},
                {"Index": 3, "Type" : "TapChanger", "Number" : 2, "BitID" : 1, "Value": 0},
                {"Index": 4, "Type" : "TapChanger", "Number" : 2, "BitID" : 2, "Value": 1},
                {"Index": 5, "Type" : "TapChanger", "Number" : 2, "BitID" : 3, "Value": 0}
            ]
            ,
            "BinaryControls" :
            [
                {"Index": 0, "Type" : "CB", "Number" : 1, "Command":"Trip"},
                {"Index": 1, "Type" : "CB", "Number" : 1, "Command":"Close"},
                {"Index": 2, "Type" : "TapChanger", "Number" : 1, "FB": "ANA", "Command":"TapUp"},
                {"Index": 3, "Type" : "TapChanger", "Number" : 1, "FB": "ANA", "Command":"TapDown"},
                {"Index": 4, "Type" : "TapChanger", "Number" : 2, "FB": "BCD", "Command":"TapUp"},
                {"Index": 5, "Type" : "TapChanger", "Number" : 2, "FB": "BCD", "Command":"TapDown"}
            ]
        } """

    # The point name definitions must match the string versions of the ODC
    # types to match.
kafkaconf = """{
    "bootstrap.servers" : "127.0.0.1:9092",
    "SocketTimeout" : 10000,
    "Topic" : "Test",

    "IP" : "127.0.0.1",
    "Port" : 9092,

    "Binary" : [	{"Index" : 1, "PITag" : "BS012340001" },
                    {"Index" : 2, "PITag" : "BS012340002"}],

    "Analog" : [	{"Index" : 0, "PITag" : "HS012340000"},
                    {"Index" : 1, "PITag" : "HS012340001"}]

    }"""

MainJSON = ""
OverrideJSON = """{"Port" : 11111,"ModuleName" : "PyPortSimModified","PollGroups" : [{"ID" : 1, "PollRate" : 11111, "Group" : 3, "PollType" : "Scan"}]}"""

# Attack the restful interface and see what we get...
def StimulateViaHttp():
    requesttimeout = 10
    try:
        r = requests.get('http://localhost:10000/PyPortRtuSim/status?Type=CB&Number=1', timeout=requesttimeout)

        print('Response is {0}'.format(json.dumps(r.json())))

        payload = {"Type" : "CB", "Number" : 1, "State" : "Fault"}

        r = requests.post('http://localhost:10000/PyPortRtuSim/set', json=payload, timeout=requesttimeout)
        print('Response is {0}'.format(r.text))

        print('Response is {0}'.format(json.dumps(r.json())))

    except requests.exceptions.RequestException as e:
        print("Exception - {}".format(e))

def SendControlTestEvent(x):
    EventType = "ControlRelayOutputBlock"
    Index = 0
    Time = 0x0000016bfd90f002
    Quality = "|ONLINE|"
    Payload = "|PULSE_ON|Count 1|ON 100ms|OFF 100ms|"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))

def SendBinaryTestEvent(x):
    EventType = "Binary"
    Index = 1
    Time = 0x0000016bfd90e5a8
    Quality = "|ONLINE|"
    Payload = "1"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))

def SendAnalogTestEvent(x):
    EventType = "Analog"
    Index = 1
    Time = 0x0000016bfd90e5a8        # 2019-07-17T01:34:20.072Z
    Quality = "|ONLINE|"
    Payload = "1"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))


# Main Section
def StimulateDirectly():
    guid = 12345678
    x = PyPortRtuSim.SimPortClass(guid, "TestInstance")
    x.Config(conf, "")
    x.Enable()
    x.Operational()

    SendBinaryTestEvent(x)

    resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortCBSim/status?CBNumber=1","")
    print("JSON Response {}".format(resjson))

    payload = {"CBNumber" : 1, "CBState" : "Closed"}
    resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortCBSim/set",json.dumps(payload))
    print("JSON Response {}".format(resjson))
    return guid, payload, resjson, x

# Kafka Test Code
def KafkaConsumeTest():
    c = Consumer({
    'bootstrap.servers': 'localhost:9092',
    'group.id': 'mygroup',
    'auto.offset.reset': 'earliest'})

    c.subscribe(['Test'])

    while True:
        msg = c.poll(1.0)

        if msg is None:
            continue
        if msg.error():
            print("Consumer error: {}".format(msg.error()))
            continue

        print('Received message: {}'.format(msg.value().decode('utf-8')))

    c.close()

pconf = {'bootstrap.servers': 'localhost:9092', 'client.id': 'OpenDataCon', 'default.topic.config': {'acks': 'all'}}
producer = Producer(pconf)

def delivery_report(err, msg):
    """ Called once for each message produced to indicate delivery result. Triggered by poll() or flush(). """
    if err is not None:
        print('Message delivery failed: {}'.format(err))
    else:
        print('Message delivered to {} [{}]'.format(msg.topic(), msg.partition()))

def KafkaProduceTest():
    # Create a dict with the key:value pairs.  The configured serialiser will
    # convert to JSON.
    # We dont actually need/use the key.  By not specifing it, the messages can
    # be spread over partitions automatically.  This
    # does mean that a consumer may process the messages out of order.  As we
    # have ODC time stamps, not really a problem.
    # Config info:
    # https://docs.confluent.io/current/clients/confluent-kafka-python/#configuration
    messagevalue = {"PITag" : "HS01234|BIN|1", "Index" : 0, "Value" : 0.0123, "Quality" : "|ONLINE|RESTART|", "TimeStamp" : "2019-07-17T01:34:20.072Z"}
    producer.produce("Test", value=json.dumps(messagevalue), callback=delivery_report)
    producer.flush()

# PyPortKakfa test Code
# Main Section
def StimulatePyPortKafkaDirectly():
    guid = 12345678
    x = PyPortKafka.SimPortClass(guid, "KafkaProducerInstance")
    x.Config(kafkaconf, "")
    x.Enable()

    SendBinaryTestEvent(x)
    x.TimerHandler(1)   # Trigger the flush command to actually send the messages.

    x.TimerHandler(1)

    resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortKafka","")
    print("JSON Response {}".format(resjson))


print("Start")
#StimulateViaHttp() # Requires the PyPortCBSim.py file to be running in OpenDataCon
StimulateDirectly() # Can use this to call code within this single Python program for simpler
# debugging and testing.
#KafkaProduceTest()
#StimulatePyPortKafkaDirectly()
#KafkaConsumeTest()
print("Done")
