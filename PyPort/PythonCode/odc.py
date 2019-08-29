# odc module, used to simulate the C++ functions exported by PyPort

# "LIs:log"
def log(guid, level, message):
    print("Log - Guid - {} - Level {} - Message - {}".format(guid, level, message))
    return

# "LsIss:PublishEvent"
# Same format as Event
def PublishEvent(guid, EventType, ODCIndex, Quality, PayLoad ):
    print("Publish Event - Guid - {} - {} {} {}".format(guid, EventType, ODCIndex, Quality, PayLoad))
    return True

# "LII:SetTimer"
def SetTimer(guid, id, milliseconds):
    print("SetTimer - Guid - {} - ID {} - Delay - {}".format(guid, id, milliseconds))
    return

# "L:GetNextEvent"
def GetNextEvent(guid):
    print("GetNextEvent - Guid - {} ".format(guid))
    return  "Binary",0,0,"|ONLINE|","1","Dummy"