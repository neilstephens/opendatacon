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

guid = 12345678
x =PyPortSim.SimPortClass(guid, "TestInstance")

MainJSON = ""
OverrideJSON = ""
x.Config(MainJSON, OverrideJSON)

x.Enable()

x.Disable()

EventType = 1
Index = 2
Time = 15
Quality = 3
Sender = "Test"
res = x.EventHandler(EventType, Index, Time, Quality, Sender)
print("Event Result - {}".format(res))

print("Done")
