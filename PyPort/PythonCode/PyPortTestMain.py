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
import re
# There is also a kafka-python library that can be used as well.  Different
# syntax though.
from confluent_kafka import Producer, Consumer, KafkaError

print("Staring Run")

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
        print('Response is {0}'.format(json.dumps(r.json())))

        payload = {"Type" : "TapChanger", "Number" : 1, "Value" : 10}
        r = requests.post('http://localhost:10000/PyPortRtuSim/set', json=payload, timeout=requesttimeout)
        print('Response is {0}'.format(json.dumps(r.json())))

    except requests.exceptions.RequestException as e:
        print("Exception - {}".format(e))

def SendBinaryTestEvent(x):
    EventType = "Binary"
    Index = 6
    Time = 0x0000016bfd90e5a8
    Quality = "|ONLINE|"
    Payload = "0"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))

def SendAnalogTestEvent(x):
    EventType = "Analog"
    Index = 0
    Time = 0x0000016bfd90e5a8        # 2019-07-17T01:34:20.072Z
    Quality = "|ONLINE|"
    Payload = "444"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))

def SendControlPulseEvent(x, Index):
    EventType = "ControlRelayOutputBlock"
    Time = 0x0000016bfd90f002
    Quality = "|ONLINE|"
    Payload = "|PULSE_ON|Count 1|ON 100ms|OFF 100ms|"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))



# Main Section
def StimulateDirectly():

    # Load the part of the config file that we need, then pass it in as a string.
    # Need to strip comments as Python json handling does not like them!
    f=open(r"C:\Users\scott\Documents\Scott\Company Work\AusGrid\example\JSON-PyPortRtuSim\opendatacon.conf", "r")
    if f.mode != 'r':
        print("Failed to open opendatacon.conf file - exiting")
        return

    confsection = f.read()
    f.close()

    confsection = re.sub(r"//\s*([^\r\n]+)", r" ", confsection)  # Remove single line comments
    jsondict = json.loads(confsection)
    rtusimsect = {}
    for d in jsondict["Ports"]:
        if d["Name"] == "PyPortRtuSim":
            # We have the one we want.
            rtusimsect = d["ConfOverrides"]

    if rtusimsect:
        guid = 12345678
        x = PyPortRtuSim.SimPortClass(guid, "TestInstance")
        x.Config(json.dumps(rtusimsect), "")
        x.Enable()
        x.Operational()

        # Circuit Breakers
        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=CB&Number=1","")
        assert (resjson == r'{"Bit0": 0, "Bit1": 1, "State": "Open"}'), "Request to get CB 1 current state failed"

        payload = {"Type" : "CB","Number" : 1, "State" : "Closed"}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to CB 1 - Closed Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=CB&Number=1","")
        assert (resjson == r'{"Bit0": 1, "Bit1": 0, "State": "Closed"}'), "Request to get CB 1 current state failed"

        SendControlPulseEvent(x,0) #Trip

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=CB&Number=1","")
        assert (resjson == r'{"Bit0": 0, "Bit1": 1, "State": "Open"}'), "Request to get CB 1 current state failed"

        SendControlPulseEvent(x,1) #Close

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=CB&Number=1","")
        assert (resjson == r'{"Bit0": 1, "Bit1": 0, "State": "Closed"}'), "Request to get CB 1 current state failed"

        # Analogs
        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Analog&Number=0","")
        assert (resjson == r'{"State": 1024}'), "Request to get Analog 0 current state failed"

        payload = {"Type" : "Analog","Number" : 0, "State" : 333}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to Analog 0 - 333 Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Analog&Number=0","")
        assert (resjson == r'{"State": 333}'), "Request to get Analog 0 current state failed"

        SendAnalogTestEvent(x)

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Analog&Number=0","")
        assert (resjson == r'{"State": 444}'), "Request to get Analog 0 current state failed"

        #Binary
        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Binary&Number=6","")
        assert (resjson == r'{"State": 0}'), "Request to get Binary 0 current state failed"

        payload = {"Type" : "Binary", "Number" : 6, "State" : 1}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to Binary 6 - 1 Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Binary&Number=6","")
        assert (resjson == r'{"State": 1}'), "Request to get Binary 6 current state failed"

        SendBinaryTestEvent(x)

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=Binary&Number=6","")
        assert (resjson == r'{"State": 0}'), "Request to get Binary 0 current state failed"

        # TapChanger 1
        payload = {"Type" : "TapChanger", "Number" : 1, "State" : 10}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to TapChanger 1 - Value 10 Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=1","")
        assert (resjson == r'{"State": 10}'), "Request to get TapChanger 1 current state failed"

        SendControlPulseEvent(x,2)  # TapUp for TC 1

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=1","")
        assert (resjson == r'{"State": 11}'), "Request to get TapChanger 1 current state failed"

        SendControlPulseEvent(x,3)  # TapDown for TC 1

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=1","")
        assert (resjson == r'{"State": 10}'), "Request to get TapChanger 1 current state failed"

        # TapChanger 2
        payload = {"Type" : "TapChanger", "Number" : 2, "State" : 12}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to TapChanger 2 - Value 12 Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 12}'), "Request to get TapChanger 2 current state failed"

        SendControlPulseEvent(x,4)  # TapUp for TC 2

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 13}'), "Request to get TapChanger 2 current state failed"

        SendControlPulseEvent(x,4)  # TapUp for TC 2
        SendControlPulseEvent(x,4)  # TapUp for TC 2

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 15}'), "Request to get TapChanger 2 current state failed"

        SendControlPulseEvent(x,4)  # TapUp for TC 2

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 15}'), "Request to get TapChanger 2 current state failed"

        SendControlPulseEvent(x,5)  # TapDown for TC 2

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 14}'), "Request to get TapChanger 2 current state failed"

        payload = {"Type" : "TapChanger", "Number" : 2, "State" : 1}
        resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortRtuSim/set",json.dumps(payload))
        assert (resjson == r'{"Result": "OK"}'), "POST to TapChanger 2 - Value 1 Failed"

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 1}'), "Request to get TapChanger 2 current state failed"

        SendControlPulseEvent(x,5)  # TapDown for TC 2

        resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortRtuSim/status?Type=TapChanger&Number=2","")
        assert (resjson == r'{"State": 1}'), "Request to get TapChanger 2 current state failed"

        # Analog Sim updates
        x.UpdateAnalogSimValues(10)
        x.UpdateAnalogSimValues(10)
        x.UpdateAnalogSimValues(60)    # This one will trigger a time out and fire off events and do change calculations

    else:
        print("Failed to find PyPortRtuSim .conf file section")

    return

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
