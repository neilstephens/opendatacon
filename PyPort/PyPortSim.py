import types
from datetime import datetime
import odc


class SimPortClass:
    ''' Our class to handle an ODC Port. We must have __init__, ProcessJSONConfig, Enable, Disable, EventHander defined, as they will be called by our c/c++ code.
    The c/c++ code also defines some methods on this class, which we can call, as well as possibly some global functions we can call.
    The class methods are PublishEvent, LogMessage.
    '''
    enabled = False # class variable shared by all instances

    def __init__(self, objectname):
        self.enabled2 = False   # class variable unique to this instance (but exists for all instances)
        self.objectname = objectname    # So we can find this instance later to call appropriate PublishEvent method

    def ProcessJSONConfig(self, MainJSON, OverrideJSON):
        """ The JSON values should be passed as strings, which we then load into a dictionary for processing"""
        print("Passed JSON Config information")
        print(MainJSON)
        print(OverrideJSON)
        
    def Enable(self):
        odc.log(1,"This is a log message "+ datetime.now().isoformat(" "))
        for d in dir():
            odc.log(2,"Dir "+ d)
        enabled = True

    def Disable(self):
        #ODC.LogMessage("Python - Disabled")
        enabled = False

    def EventHandler(self,EventType, Index, Time, Quality, Sender):
        print("EventHander: {}, {}".format(Sender, self.enabled2))