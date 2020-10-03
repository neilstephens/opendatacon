## Table of contents

* [Introduction](#introduction)
    * [Features](#features)
		* [What it does do](#What-it-does-do)
		* [What it does not do](#What-it-does-not-do)
	* [Unit Tests](#unit-tests)
	* [Internal data structure](#internal-data-structure)
	* [Configuration files and syntax](#configuration-files-and-syntax)


## Introduction

MD3Port is a MD3 Port implementation for OpenDataCon.

## Features

MD3Port implements both a Master and Outstation OpenDataCon port, which talks over a TCP connection to Outstations or Scada Masters that use the MD3 RTU protocols.
It supports multi-drop configuration for both Master and Outstation using the Station Address field (1 to 253) to separate traffic. Address 0 is global broadcast.

### What it does do
The following function codes are supported:

| Function Code | Action | Implemented |
|---------------|--------|------------|
|5|ANALOG_UNCONDITIONAL| Yes |
|6|ANALOG_DELTA_SCAN|  Yes |
|7|DIGITAL_UNCONDITIONAL_OBS | Yes, Outstation but Obsolete Code |
|8|DIGITAL_DELTA_SCAN| Yes, Outstation but Obsolete Code | Yes, Outstation but Obsolete Code |
|9|HRER_LIST_SCAN| Yes, Outstation but Obsolete Code |
|10S|DIGITAL_CHANGE_OF_STATE| Yes, Outstation but Obsolete Code |
|11|DIGITAL_CHANGE_OF_STATE_TIME_TAGGED| Yes |
|12|DIGITAL_UNCONDITIONAL| Yes |
|13|ANALOG_NO_CHANGE_REPLY| Yes |
|14|DIGITAL_NO_CHANGE_REPLY| Yes |
|15|CONTROL_REQUEST_OK| Yes |
|16|FREEZE_AND_RESET| Yes |
|17|POM_TYPE_CONTROL| Yes |
|19|DOM_TYPE_CONTROL| Yes |
|20|INPUT_POINT_CONTROL| Yes, Station Only ATM|
|21|RAISE_LOWER_TYPE_CONTROL|No |
|23|AOM_TYPE_CONTROL| Yes |
|30|CONTROL_OR_SCAN_REQUEST_REJECTED| Yes |
|31|COUNTER_SCAN| Yes |
|40|SYSTEM_SIGNON_CONTROL| Yes |
|41|SYSTEM_SIGNOFF_CONTROL| No |
|42|SYSTEM_RESTART_CONTROL| No |
|43|SYSTEM_SET_DATETIME_CONTROL| Yes |
|44|SYSTEM_SET_DATETIME_CONTROL_NEW| Yes |
|50|FILE_DOWNLOAD| No |
|51|FILE_UPLOAD| No |
|52|SYSTEM_FLAG_SCAN| Yes |
|60|LOW_RES_EVENTS_LIST_SCAN| No |

Connections are over TCP, with the Outstation typically a server and the Master typically a client. It can be changed in the config file.


For support of COS (Change of State Events - Binary), if a Binary event comes in from ODC (ie we are an outstation) and it is OLDER than the last received event for that ODC binary point,
it will be put into the COS queue, but will not result in a change to the current value of that binary (which would be reported to the Master on the next successful scan)
When we are a Master and we issue and then process an COS scan, each COS binary point data will trigger Binary ODC events, with the corresponding timestamps.
They may be out of order or older than the last value returned by a function 0 scan.
We are relying on the port implementation on the other side of the ODC connection to ignore older events (or put them in an COS equivalent list)
CBPort and MD3Port both do this.

#### Master Scanning
The scans are done with analog or binary scans. The Counter values can be read wit anlog scan commands and vice versa.

### What it does not do
There is no serial interface. TCP connections only.

## Unit Tests
If you are building under Visual Studio, there is a define (#define NONVSTESTING) that if turned OFF will allow you to have the unit tests appear in the VS Test Explorer.
The version of Catch, catchvs.hpp that allows this to happen is no longer supported, and has some bugs - not all tests will run as a group, but will run individually.
It is very useful in debugging and development!
When this define is turned ON, there is also a separate test program/project that can run all the tests using a "normal" version of catch, by calling a function in the CBPort dll, and you can still run this in VS and set breakpoints etc.


## Configuration files and syntax

Starting with an example configuration:

```json
    "IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 124,
	"ServerType" : "PERSISTENT",
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	//-------Point conf--------#
	// We have two modes for the digital/binary commands. Can be one or the other - not both!
	"NewDigitalCommands" : true,

	// If a binary event time stamp is outside 30 minutes of current time, replace the timestamp
	"OverrideOldTimeStamps" : false,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for MD3 Master responses to a command and number of times to retry a command.
	"MD3CommandTimeoutmsec" : 3000,
	"MD3CommandRetries" : 1,

	// The magic points we use to pass through MD3 commands. 0 is the default value. If set to 0, they are inactive.
	// If we set StandAloneOutStation above, and use the pass through's here, we can maintain MD3 type packets through ODC.
	// It is not however then possible to do a MD3 to DNP3 connection that will work.
	"TimeSetPoint" : {"Index" : 0},
	"TimeSetPointNew" : {"Index" : 0},
	"SystemSignOnPoint" : {"Index" : 0},
	"FreezeResetCountersPoint"  : {"Index" : 0},
	"POMControlPoint" : {"Index" : 0},
	"DOMControlPoint" : {"Index" : 0},

	// Master only PollGroups
	// Cannot mix analog and binary points in a poll group. Group 1 is Binary, Group 2 is Analog in this example
	// You will get errors if the wrong type of points are assigned to the wrong poll group
	// The force unconditional parameter will force the code to NOT use delta commands during polling.
	// The TimeSetCommand is used to send time set commands to the OutStation
	// The "TimeTaggedDigital" flag if true, asks for COS records in addition to "Normal" digital values in the scan (using Fn11)
	// When we scan digitals, we do a scan for each module. The logic of scanning multiple modules gets a little tricky.
	// The digital scans (what is scanned) is worked out from the Binary point definition. The first module address,
	// the total number of modules which are part of the Fn11 and 12 commands

	"PollGroups" : [{"PollRate" : 10000, "ID" : 1, "PointType" : "Binary", "TimeTaggedDigital" : true },
					{"PollRate" : 20000, "ID" : 2, "PointType" : "Analog", "ForceUnconditional" : false },
					{"PollRate" : 60000, "ID" : 3, "PointType" : "TimeSetCommand"},
					{"PollRate" : 60000, "ID" : 4, "PointType" : "Binary",  "TimeTaggedDigital" : false },
					{"PollRate" : 180000, "ID" : 5, "PointType" : "SystemFlagScan"},
					{"PollRate" : 60000, "ID" : 6, "PointType" : "NewTimeSetCommand"},
					{"PollRate" : 60000, "ID" : 7, "PointType" : "Counter"},
					{"PollRate" : 60000, "ID" : 8, "PointType" : "Counter"}],

	// We expect the binary modules to be consecutive - MD3 Addressing - (or consecutive groups for scanning), the scanning is then simpler
	"Binaries" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 34, "Offset" : 0, "PollGroup" : 1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 16, "Stop" : 31}, "Module" : 35, "Offset" : 0, "PollGroup":1, "PointType" : "TIMETAGGEDINPUT"},
				{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PollGroup":4,"PointType" : "BASICINPUT"},
				{"Range" : {"Start" : 32, "Stop" : 47}, "Module" : 63, "Offset" : 0, "PointType" : "TIMETAGGEDINPUT"}],

	"Analogs" : [{"Range" : {"Start" : 0, "Stop" : 15}, "Module" : 32, "Offset" : 0, "PollGroup" : 2}],

	"BinaryControls" : [{"Index": 80,  "Module" : 33, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Range" : {"Start" : 100, "Stop" : 115}, "Module" : 37, "Offset" : 0, "PointType" : "DOMOUTPUT"},
						{"Range" : {"Start" : 116, "Stop" : 123}, "Module" : 38, "Offset" : 0, "PointType" : "POMOUTPUT"}],

	"Counters" : [{"Range" : {"Start" : 0, "Stop" : 7}, "Module" : 61, "Offset" : 0, "PollGroup" : 7},
					{"Range" : {"Start" : 8, "Stop" : 15}, "Module" : 62, "Offset" : 0, "PollGroup" : 8}],

	"AnalogControls" : [{"Range" : {"Start" : 1, "Stop" : 8}, "Module" : 39, "Offset" : 0}]