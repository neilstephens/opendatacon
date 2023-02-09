import json
import sys
from datetime import datetime, timedelta
import odc
import paramiko

# Logging Levels
Trace = 0
Debug = 1
Info = 2
Warn = 3
Error = 4
Critical = 5

# Exact string values for Event parameters which are passed as strings
# EventTypes, ConnectState,Binary,Analog,Counter,FrozenCounter,BinaryOutputStatus,AnalogOutputStatus,ControlRelayOutputBlock and others...
# QualityFlags, ONLINE,RESTART,COMM_LOST,REMOTE_FORCED,LOCAL_FORCE,OVERRANGE,REFERENCE_ERR,ROLLOVER,DISCONTINUITY,CHATTER_FILTER
# ConnectState, PORT_UP,CONNECTED,DISCONNECTED,PORT_DOWN
# ControlCode, NUL,NUL_CANCEL,PULSE_ON,PULSE_ON_CANCEL,PULSE_OFF,PULSE_OFF_CANCEL,LATCH_ON,LATCH_ON_CANCEL,LATCH_OFF,LATCH_OFF_CANCEL,
#               CLOSE_PULSE_ON,CLOSE_PULSE_ON_CANCEL,TRIP_PULSE_ON,TRIP_PULSE_ON_CANCEL,UNDEFINED

class SftpPortClass:
    ''' Our class to handle an ODC Port. We must have __init__, ProcessJSONConfig, Enable, Disable, EventHander, TimerHandler and
    RestRequestHandler defined, as they will be called by our c/c++ code.
    ODC publishes some functions to this Module (when run) they are part of the odc module(include).
    We currently have odc.log, odc.SetTimer and odc.PublishEvent.
    '''

    # Worker Methods. They need to be high in the code so they are available in the code below. No forward declaration in Python
    def LogTrace(self, message ):
        odc.log(self.guid, Trace, message )
    def LogError(self, message ):
        odc.log(self.guid,Error, message )
    def LogDebug(self, message ):
        odc.log(self.guid, Debug, message )
    def LogInfo(self, message ):
        odc.log(self.guid,Info, message )
    def LogWarn(self, message ):
        odc.log(self.guid, Warn, message )
    def LogCritical(self, message ):
        odc.log(self.guid,Critical, message )


    # Required Method
    def __init__(self, odcportguid, objectname):
        self.objectname = objectname            # Documentation/error use only.
        self.guid = odcportguid                 # So that when we call an odc method, ODC can work out which pyport to hand it too.
        self.Enabled = False
        self.ConfigDict = {}                    # Config Dictionary
        self.binary = {}                        # Status and timstamp of events
        self.pre_event_data_min = 5             # Minutes of pre-event data required
        self.post_event_data_min = 20           # Minutes of post-event data required
        self.file_retrieve_interval_hours = 1   # How often to retrieve files
        self.file_retrieval_hour_offset = 30    # Hour offset for file retrieval
        self.file_list = []                     # List containing files to retrieve
        self.LogDebug("*********** SimPortClass Init Called - File Version 1 - {}".format(objectname))
        return


    # Required Method
    def Config(self, MainJSON, OverrideJSON):
        """ The JSON values are passed as strings (stripped of comments), which we then load into a dictionary for processing
        Note that this does not handle Inherits JSON entries correctly (Inherits is effectily an Include file entry)"""

        # Load JSON into Dicts
        Override = {}
        try:
            if len(MainJSON) != 0:
                self.ConfigDict = json.loads(MainJSON)
            if len(OverrideJSON) != 0:
                Override = json.loads(OverrideJSON)
        except:
           self.LogError("Exception on parsing JSON Config data - {}".format(sys.exc_info()[0]))
           return

        self.LogDebug("JSON Config strings Parsed")

        # Now use the override config settings to adjust or add to the MainConfig. Only root json values can be adjusted.
        # So you cannot change a single value in a Binary point definition without rewriting the whole "Binaries" json key.
        self.ConfigDict.update(Override)               # Merges with Override doing just that - no recursion into sub dictionaries

        self.LogDebug("Combined (Merged) JSON Config {}".format(json.dumps(self.ConfigDict)))

        # Now extract what is needed for this instance, or just reference the ConfigDict when needed.
        return
        

    def TimerDuration(self):
        # determine time until next file retrieval
        # returns timer duration in milliseconds
        now = datetime.now()
        target_time = now.replace(
                            hour = now.hour + self.file_retrieve_interval_hours, 
                            minute = self.file_retrieval_hour_offset,
                            second = 0
                            )
        time_diff = target_time - now
        time_diff_ms = time_diff.seconds*1000
        return time_diff_ms


    def GetFilesSFTP(self):
        self.LogDebug("Retrieving files...")
        self.LogDebug("File list (pre): {}".format(self.file_list))
        
        try:
            # set up connection
            transport = paramiko.Transport((self.SFTPConf["IP"], self.SFTPConf["Port"]))
            transport.connect(username = self.SFTPConf["Username"], password = self.SFTPConf["Password"])
            sftp = paramiko.SFTPClient.from_transport(transport)
        except Exception as e:
            self.LogDebug("Error connecting to {}".format(self.SFTPConf["Id"]))
            self.LogDebug("{}".format(e)) 
            sftp = False

        # retrieve file for each timestamp stored
        if sftp:
            remove_list = []
            for timestamp in self.file_list:
                # check to see that file recording period is complete
                # before attempting to retrieve file
                if datetime.now() > (timestamp + timedelta(hours=1)):
                    try:
                        # remote file uses unix timestamp
                        get_file = "{}-{}.csv".format(
                                                    self.SFTPConf["Id"], 
                                                    int(timestamp.timestamp())
                                                    )
                        # save file locally with human readable timestamp
                        put_file = "{}-{}.csv".format(
                                                    self.SFTPConf["Id"], 
                                                    timestamp.strftime('%Y-%m-%d_%H-%M-%S')
                                                    )
                        # retrieve the file
                        sftp.get(
                            self.SFTPConf["RemotePath"] + get_file, 
                            self.SFTPConf["LocalPath"] + put_file
                            )
                        
                        self.LogDebug("Retrieved file: {}".format(put_file))
                        remove_list.append(timestamp)

                    except FileNotFoundError:
                        # log exception if file can't be retrieved
                        self.LogDebug("Could not find {} ({})".format(get_file, put_file))
                        remove_list.append(timestamp)

            for timestamp in remove_list:
                self.LogDebug("Removing {} from file list".format(timestamp))
                self.file_list.remove(timestamp)     

            # update backup file
            with open('list_backup', 'w') as f:
                f.write(json.dumps([x.isoformat() for x in self.file_list]))

        self.LogDebug("File list (post): {}".format(self.file_list)) 

        return


    # Required Method
    def Operational(self):
        """ This is called from ODC once ODC is ready for us to be fully operational - normally after Build is complete"""
        self.LogDebug("Port Operational - {}".format(datetime.now().isoformat(" ")))

        if "SFTPConf" in self.ConfigDict:
            self.SFTPConf = self.ConfigDict["SFTPConf"]
            self.LogDebug(str(self.SFTPConf))

        # initialise the status of the binaries to be monitored
        for index in self.SFTPConf["IndexList"]:
            self.binary[index] = {'status': False, 'start_time': None}

        try:
            with open('list_backup', 'r') as f:
                file_data = json.loads(f.read())
                self.file_list = [datetime.strptime(x, "%Y-%m-%dT%H:%M:%S") for x in file_data]
            self.LogDebug("Port Operational - File list loaded from backup file: {}".format(file_data))
        except:
            self.LogDebug("Port Operational - no backup list to load")

        # start the file retrieval timer
        timer_duration_ms = self.TimerDuration()
        odc.SetTimer(self.guid, 1, timer_duration_ms) 
        next_timer = datetime.now()+timedelta(seconds=timer_duration_ms/1000)
        self.LogDebug("Next file retrieval: {}".format(next_timer.isoformat(" ")))

        return


    # Required Method
    def Enable(self):
        self.LogTrace("Enabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = True
        return


    # Required Method
    def Disable(self):
        self.LogDebug("Disabled - {}".format(datetime.now().isoformat(" ")))
        self.enabled = False
        return


    # Needs to return True or False, which will be translated into CommandStatus::SUCCESS or CommandStatus::UNDEFINED
    # EventType (string) Index (int), Time (msSinceEpoch), Quality (string) Payload (string) Sender (string)
    # There is no callback available, the ODC code expects this method to return without delay.
    def EventHandler(self,EventType, Index, Time, Quality, Payload, Sender):
        self.LogTrace("EventHander: {}, {}, {} {} - {}".format(self.guid,Sender,Index,EventType,Payload))

        # Binary payload = 1
        if (EventType == "Binary" and Index in self.binary and Payload == "1"):
            if self.binary[Index]['status']:
                # This should not happen - event already active
                self.LogDebug("EventHander: Binary event is already active. Binary: {}, Status: {}, Payload: {}".format(Index, self.binary[Index]['status'], Payload))
            else:
                self.LogDebug("EventHandler: Binary event started for index {}".format(Index))
                self.binary[Index]['status'] = True
                self.binary[Index]['start_time'] = datetime.now()
                self.LogDebug("{}".format(self.binary))

        # Binary payload = 0
        elif (EventType == "Binary" and Index in self.binary and Payload == "0"):
            if self.binary[Index]['status']:
                duration = datetime.now() - self.binary[Index]['start_time']
                self.LogDebug("EventHandler: Binary event finished for index {}. Start: {}, Duration: {}".format(Index, self.binary[Index]['start_time'], duration))
                # calculate time period of data required
                data_end_time = datetime.now() + timedelta(minutes=self.post_event_data_min)
                data_start_time = self.binary[Index]['start_time'] - timedelta(minutes=self.pre_event_data_min)

                # add file datetimes to file retrieval list (i.e. list of datetimes)
                file_datetime = data_start_time.replace(minute = 0, second = 0, microsecond=0) 
                while data_end_time > file_datetime:
                    if file_datetime not in self.file_list:
                        self.file_list.append(file_datetime)
                        self.LogDebug("EventHandler: {} added to file list".format(file_datetime.strftime('%Y-%m-%d_%H-%M-%S')))
                    else:
                        self.LogDebug("EventHandler: {} already in file list".format(file_datetime.strftime('%Y-%m-%d_%H-%M-%S')))
                    file_datetime = file_datetime + timedelta(hours=1)  
                
                # update backup file
                with open('list_backup', 'w') as f:
                    f.write(json.dumps([x.isoformat() for x in self.file_list]))

                # set event status to false
                self.binary[Index]['status'] = False
                self.binary[Index]['start_time'] = None
                self.LogDebug("{}".format(self.binary))
            else:
                # This should not happen - no event was active
                self.LogDebug("EventHander: Binary event is not active. Binary: {}, Status: {}, Payload: {}".format(Index, self.binary[Index]['status'], Payload))

        return True


    # Will be called at the appropriate time by the ASIO handler system. Will be passed an id for the timeout,
    # so you can have multiple timers running.
    def TimerHandler(self,TimerId):
        self.LogTrace("TimerHander: ID {}, {}".format(TimerId, self.guid))
        
        if (TimerId == 1):
            # retrieve files
            self.GetFilesSFTP()

            # set up to run again
            timer_duration_ms = self.TimerDuration()
            odc.SetTimer(self.guid, 1, timer_duration_ms)

            next_timer = datetime.now()+timedelta(seconds=timer_duration_ms/1000)
            self.LogDebug("Next file retrieval: {}".format(next_timer.isoformat(" ")))

        return


    # The Rest response interface - the following method will be called whenever the restful interface (a single interface for all PythonPorts) gets
    # called. It will be decoded sufficiently so that it is passed to the correct PythonPort (us)
    # To make these calls in Python (our test scripts) we can use the library below.
    # https://2.python-requests.org//en/master/
    #
    # We return the response that we want sent back to the caller. This will be a JSON string. A null string would be an error.
    def RestRequestHandler(self, url, content):
        self.LogTrace("RestRequestHander: {}".format(url))      

        return