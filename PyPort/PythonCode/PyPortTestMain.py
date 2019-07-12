# This is a test scaffold for the PyPort Python template code.
# We create scaffolding so that we can build and run the Python code outside of ODC PyPort.
# Allows us to make sure everything works before we load it into ODC

# To support this we need an odc module to import (that simulates what odc offers), and a main function to
# create the Python class (like SimPortClass) and then call the methods with suitable test arguments
import odc
#import PyPortSim
import PyPortCBSim
import requests
import json

print("Staring Run")

conf = """{"Binaries": [{"CBNumber": 1, "Index": 0, "SimType": "CBStateBit0", "State": 0}, {"CBNumber": 1, "Index": 1, "SimType": "CBStateBit1", "State": 1}],
    "BinaryControls": [{"CBCommand": "Trip", "CBNumber": 1, "Index": 0}, {"CBCommand": "Close", "CBNumber": 1, "Index": 1}], 
    "ClassName": "SimPortClass", "IP": "localhost", "ModuleName": "PyPortCBSim", "Port": 10000} """

MainJSON = ""
OverrideJSON = """{"Port" : 11111,"ModuleName" : "PyPortSimModified","PollGroups" : [{"ID" : 1, "PollRate" : 11111, "Group" : 3, "PollType" : "Scan"}]}"""

# Attack the restful interface and see what we get...
def StimulateViaHttp():
    requesttimeout = 10
    try:
        r = requests.get('http://localhost:10000/PyPortCBSim/status?CBNumber=1', timeout=requesttimeout)

        print('Response is {0}'.format(json.dumps(r.json())))

        payload = {"CBNumber" : 1, "CBState" : "Fault"} 
    
        r = requests.post('http://localhost:10000/PyPortCBSim/set', json=payload, timeout=requesttimeout)
        print('Response is {0}'.format(r.text))

        print('Response is {0}'.format(json.dumps(r.json())))
                 
    except requests.exceptions.RequestException as e:
        print("Exception - {}".format(e))

def SendTestEvent(x):
    EventType = "ControlRelayOutputBlock"
    Index = 0
    Time = 15
    Quality = "|ONLINE|"
    Payload = "|PULSE_ON|Count 1|ON 100ms|OFF 100ms|"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))


# Main Section
def StimulateDirectly():
    guid = 12345678
    x = PyPortCBSim.SimPortClass(guid, "TestInstance")    
    x.Config(conf, "")    
    x.Enable()
    
    SendTestEvent(x)

    resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortCBSim/status?CBNumber=1","")
    print("JSON Response {}".format(resjson))
    
    payload = {"CBNumber" : 1, "CBState" : "Closed"}
    resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortCBSim/set",json.dumps(payload))
    print("JSON Response {}".format(resjson))
    return guid, payload, resjson, x


print("Start")
#StimulateViaHttp()  # Requires the PyPortCBSim.py file to be running in OpenDataCon
StimulateDirectly() # Can use this to call code within this single Python program for simpler debugging and testing.
print("Done")


