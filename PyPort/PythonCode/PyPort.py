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

guid = 12345678
x =PyPortSim.SimPortClass(guid, "TestInstance")

MainJSON = ""
OverrideJSON = """{"Port" : 11111,"ModuleName" : "PyPortSimModified","PollGroups" : [{"ID" : 1, "PollRate" : 11111, "Group" : 3, "PollType" : "Scan"}]}"""
x.Config(conf, OverrideJSON)

x.Enable()

x.Disable()

EventType = 1    # Binary?
Index = 2
Time = 15
Quality = "|RESTART|"
Payload = "1"
Sender = "Test"
res = x.EventHandler(EventType, Index, Time, Quality, Payload, Sender)
print("Event Result - {}".format(res))

resjson = x.RestRequestHandler("http:\\\\testserver\\name\\cboperation?Fail=true")
print("JSON Response {}".format(resjson))

x.TimerHandler(1)

print("Done")
