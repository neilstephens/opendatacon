class SimPortClass:
    enabled = False # class variable shared by all instances

    def __init__(self, objectname):
        self.enabled2 = False   # class variable unique to this instance (but exists for all instances)
        self.objectname = objectname    # So we can find this instance later to call appropriate PublishEvent method

    def Enable(self):
        print("Python - Enabled")
        enabled = True

    def Disable(self):
        print("Python - Disabled")
        enabled = False

    def EventHandler(self,EventType, Index, Time, Quality, Sender):
        print("EventHander: {}, {}".format(Sender, self.enabled2))