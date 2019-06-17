# odc module, used to simulate the C++ functions exported by PyPort

# "LIs:log"
def log(guid, level, message):
    print("Log - Guid - {} - Level {} - Message - {}".format(guid, level, message))
    return

# "LIIIO:PublishEvent"
def PublishEvent(guid, ODCIndex, value, quality, pyTime):
    print("Publish Event - Guid - {} - {} {} {} {}".format(guid, ODCIndex, value, quality, pyTime))
    return True

# "LII:SetTimer"
def SetTimer(guid, id, milliseconds):
    print("SetTimer - Guid - {} - ID {} - Delay - {}".format(guid, id, milliseconds))
    return