# This is a test scaffold for the PyPort Python template code.
# We create scaffolding so that we can build and run the Python code outside of
# ODC PyPort,
# so we can make sure everything works before we load it into ODC

# To support this we need an odc module to import, and a main function to
# create the Python class (like SimPortClass) and then call the methods with
# suitable
# test arguments
import odc
import PyPortSim
import PyPortCBSim
import requests
import json

print("Staring Run")

conf = """{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"ModuleName" : "PyPortSim",
	"ClassName": "SimPortClass",
	"PollGroups" : [{"ID" : 1, "PollRate" : 10000, "Group" : 3, "PollType" : "Scan"},
					{"ID" : 2, "PollRate" : 20000, "Group" : 3, "PollType" : "SOEScan"},
					{"ID" : 3, "PollRate" : 120000, "PollType" : "TimeSetCommand"}],
	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 1, "Type" : "DIG", "SOE" : {"Group": 5, "Index" : 0} },
					{"Index" : 12, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA"},
					{"Index" : 13, "Group" : 3, "PayloadLocation": "2A", "Channel" : 2, "Type" : "MCB"},
					{"Index" : 14, "Group" : 3, "PayloadLocation": "2A", "Channel" : 3, "Type" : "MCC" }],
	"Analogs" : [	{"Index" : 0, "Group" : 3, "PayloadLocation": "3A","Channel" : 1, "Type":"ANA"},
					{"Index" : 1, "Group" : 3, "PayloadLocation": "3B","Channel" : 1, "Type":"ANA"},
					{"Index" : 2, "Group" : 3, "PayloadLocation": "4A","Channel" : 1, "Type":"ANA"},
					{"Index" : 3, "Group" : 3, "PayloadLocation": "4B","Channel" : 1, "Type":"ANA6"},
					{"Index" : 4, "Group" : 3, "PayloadLocation": "4B","Channel" : 2, "Type":"ANA6"}],
	"BinaryControls" : [{"Index": 1,  "Group" : 4, "Channel" : 1, "Type" : "CONTROL"},
                        {"Range" : {"Start" : 10, "Stop" : 21}, "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],
	"AnalogControls" : [{"Index": 1,  "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}]

}"""


conf2 = """{"Binaries": [{"CBNumber": 1, "Index": 0, "SimType": "CBStateBit0", "State": 0}, {"CBNumber": 1, "Index": 1, "SimType": "CBStateBit1", "State": 1}],
    "BinaryControls": [{"CBCommand": "Trip", "CBNumber": 1, "Index": 0}, {"CBCommand": "Close", "CBNumber": 1, "Index": 1}], 
    "ClassName": "SimPortClass", "IP": "localhost", "ModuleName": "PyPortCBSim", "Port": 10000} """

MainJSON = ""
OverrideJSON = """{"Port" : 11111,"ModuleName" : "PyPortSimModified","PollGroups" : [{"ID" : 1, "PollRate" : 11111, "Group" : 3, "PollType" : "Scan"}]}"""

# Attack the restful interface and see what we get...
def StimulateViaHttp():
    requesttimeout = 10
    try:
#        r = requests.get('http://localhost:10000/PyPortCBSim/status?CBNumber=1', timeout=requesttimeout)

 #       print('Response is {0}'.format(json.dumps(r.json())))

        payload = {"CBNumber" : 1, "CBState" : "Close"} 
    
        r = requests.post('http://localhost:10000/PyPortCBSim/set', json=payload, timeout=requesttimeout)
        print('Response is {0}'.format(r.text))

        print('Response is {0}'.format(json.dumps(r.json())))
                 
    except requests.exceptions.RequestException as e:
        print("Exception - {}".format(e))

def SendTestEvent(x):
    EventType = 16    # Binary?
    Index = 0
    Time = 15
    Quality = "|ONLINE|"
    Payload = "1"
    Sender = "Test"
    res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
    print("Event Result - {}".format(res))


# Main Section
def StimulateDirectly():
    guid = 12345678
    x = PyPortCBSim.SimPortClass(guid, "TestInstance")
    
    x.Config(conf2, "")
    
    x.Enable()
    
    resjson = x.RestRequestHandler("GET http://localhost:10000/PyPortCBSim/status?CBNumber=1","")
    print("JSON Response {}".format(resjson))
    
    payload = {"CBNumber" : 1, "CBState" : "Open"}
    resjson = x.RestRequestHandler("POST http://localhost:10000/PyPortCBSim/ ",json.dumps(payload))
    print("JSON Response {}".format(resjson))
    return guid, payload, resjson, x


print("Start")
StimulateViaHttp()
print("Done")


