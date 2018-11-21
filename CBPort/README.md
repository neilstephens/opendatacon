## Table of contents

* [Introduction](#introduction)
    * [Features](#features)
		* [What it does do](#What-it-does-do)
		* [What it does not do](#What-it-does-not-do)
	* [Unit Tests](#unit-tests)
	* [Internal data structure](#internal-data-structure)
	* [Configuration files and syntax](#configuration-files-and-syntax)


## Introduction

CBPort is a Conitel/Baker Port implementation for OpenDataCon.

## Features

CBPort implements both a Master and Outstation OpenDataCon port, which talks over a TCP connection to Outstations or Scada Masters that use the Conitel or Baker RTU protocols.
It supports multi-drop configuration for both Master and Outstation using the Station Address field (1 to 15) to separate traffic. Address 0 is global broadcast.
The only difference between the protocols is the switching of the Station Address and the Group value.

### What it does do
The following function codes are supported:

| Function Code | Action |
|---------------|--------|
|	'0'	| Scan data	|
|	'1'	| Execute	|
|	'2' | Trip		|
|	'4' | Close		|
|	'3'	| SetPoint A	|
|	'5'	| SetPoint B	|
|	'9' | Master station request	|
|	'10'| Send New SOE |
|	'11'| Send Last SOE	|

Master Station Request (9) has a bunch of sub codes, listed below:

| Master SubFunction Code | Action | Implemented |
|-------------------------|--------|-------------|
|	'0'	| Not Used	| No |
|	'1'	| Test RAM	| Yes - We respond with ALL OK |
|	'2'	| Test PROM	| Yes - We respond with ALL OK |
|	'3'	| Test EPROM	| Yes - We respond with ALL OK |
|	'4'	| Test IO	| Yes - We respond with ALL OK |
|	'5'	| Download Data -> Send Time Updates	| Yes - We respond but dont change our time |
|	'6'	| Spare 1	| No |
|	'7'	| Spare 2	| No |
|	'8'	| Retrieve Remote Status Word | We reply with all OK |
|	'9'	| Retrieve Input Circuit Data	| No |
|	'10'	| Time Correction Factor Establishment	| Yes - We respond but dont change our time |
|	'11'	| Repeat Previous Transmission	| Yes |
|	'12'	| Spare 3	| No |
|	'13'	| Set Loop backs	| No |
|	'14'	| Reset RTU Warm	| No |
|	'15'	| Reset RTU Cold	| No |

Connections are over TCP, with the Outstation typically a server and the Master typically a client. It can be changed in the config file.

The Trip/Close (Binary control) and SetPointA/B (Analog Control) functions are a two stage processes where they are sent, but need to be followed by an Execute command.

For support of SOE (Sequence of Events - Binary), if a Binary event comes in from ODC (ie we are an outstation) and it is OLDER than the last received event for that ODC binary point,
it will be put into the SOE queue, but will not result in a change to the current value of that binary (which would be reported to the Master on the next successful scan)
When we are a Master and we issue and then process an SOE scan, each SOE binary point data will trigger Binary ODC events, with the corresponding timestamps.
They may be out of order or older than the last value returned by a function 0 scan.
We are relying on the port implementation on the other side of the ODC connection to ignore older events (or put them in an SOE equivalent list)
CBPort and MD3Port both do this.

#### Master Scanning
In order to get data from an Outstation, the Master needs to poll the Outstation(s).
In the config file we can set up poll groups (0 to 15) and then define what data will appear in what payload locations. Up to 16 blocks - 31 x 12 bit payload locations can be
returned in one scan of one group address.
The payload locations are 1B, 2A, 2B, 3A, 3B etc up to 16B. (1A is not a payload location)
Binary, Analog and Status information can all be returned in one scan group. Data can be mapped to multiple scan groups.
There is no information carried in the scan packet as to the contents of the data. The Master and Outstation must be configured identically for the information passed between them to make sense.
The Master can also configure a poll for an SOE scan. This type of scan also uses a group number (0 to 8 - only the 3 least significant bits are used) and includes an SOE point number (0-120), which we map to an ODC binary point.
Again this point must match what is configured in the non-ODC outstation/master.

#### Master Set Commands
These will be triggered by the Master receiving an ODC control event. The Trip/Close are for ControlRelayOutputBlock, SetpointA/B for AnalogOutputInt16. There is support for 1 to 15 group addresses, with 12 binary control points per group, and 2 analog points per group (A/B)

### What it does not do
The following function codes are NOT supported:

| Function Code | Action |
|---------------|--------|
|	'8'	| Reset RTU	|
|	'13'	| Raise/Lower	|
|	'14'	| Freeze and Scan Accumulator	|
|	'15'	| Freeze, Scan and Reset Accumulator	|

There is no serial interface. TCP connections only.

## Unit Tests
If you are building under Visual Studio, there is a define (#define NONVSTESTING) that if turned OFF will allow you to have the unit tests appear in the VS Test Explorer.
The version of Catch, catchvs.hpp that allows this to happen is no longer supported, and has some bugs - not all tests will run as a group, but will run individually.
It is very useful in debugging and development!
When this define is turned ON, there is also a separate test program/project that can run all the tests using a "normal" version of catch, by calling a function in the CBPort dll, and you can still run this in VS and set breakpoints etc.


## Internal data structure

TODO

## Configuration files and syntax

Starting with an example configuration:

```json
{
	"IP" : "127.0.0.1",
	"Port" : 10000,
	"OutstationAddr" : 9,
	"TCPClientServer" : "SERVER",
	"LinkNumRetry": 4,

	"IsBakerDevice" : false,

	// If a binary event time stamp is outside 30 minutes of current time, replace the timestamp
	"OverrideOldTimeStamps" : false,

	// This flag will have the OutStation respond without waiting for ODC responses - it will still send the ODC commands, just no feedback. Useful for testing and connecting to the sim port.
	// Set to false, the OutStation will set up ODC/timeout callbacks/lambdas for ODC responses. If not found will default to false.
	"StandAloneOutstation" : true,

	// Maximum time to wait for CB Master responses to a command and number of times to retry a command.
	"CBCommandTimeoutmsec" : 3000,
	"CBCommandRetries" : 1,

	// Master only PollGroups - ignored by outstation
	"PollGroups" : [{"ID" : 1, "PollRate" : 10000, "Group" : 3, "PollType" : "Scan"}],

	//-------Point conf--------#
	// The payload location can be 1B, 2A, 2B
	// Where there is a 24 bit result (ACC24) the next payload location will automatically be used. Do not put something else in there!
	// The point table will build a group list with all the data it has to collect for a given group number.
	// We can only use range for Binary and Control. For analog each one has to be defined singularly

	// Digital IN
	// DIG - 12 bits to a Payload, Channel(bit) 1 to 12. On a range, the Channel is the first Channel in the range.
	// MCA,MCB,MCC - 6 bits to a payload.

	// Analog IN
	// ANA - 1 Channel to a payload,

	// Counter IN
	// ACC12 - 1 to a payload,
	// ACC24 - takes two payloads.

	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "Group" : 3, "PayloadLocation": "1B", "Channel" : 1, "Type" : "DIG", "SOE" : true, "SOEIndex" : 0},
					{"Index" : 12, "Group" : 3, "PayloadLocation": "2A", "Channel" : 1, "Type" : "MCA", "SOE" : true, "SOEIndex" : 20},
					{"Index" : 13, "Group" : 3, "PayloadLocation": "2A", "Channel" : 2, "Type" : "MCB", "SOE" : false},
					{"Index" : 14, "Group" : 3, "PayloadLocation": "2A", "Channel" : 3, "Type" : "MCC" }],

	"Analogs" : [	{"Index" : 0, "Group" : 3, "PayloadLocation": "3A","Channel" : 1, "Type":"ANA"},
					{"Index" : 1, "Group" : 3, "PayloadLocation": "3B","Channel" : 1, "Type":"ANA"},
					{"Index" : 2, "Group" : 3, "PayloadLocation": "4A","Channel" : 1, "Type":"ANA"},
					{"Index" : 3, "Group" : 3, "PayloadLocation": "4B","Channel" : 1, "Type":"ANA6"},
					{"Index" : 4, "Group" : 3, "PayloadLocation": "4B","Channel" : 2, "Type":"ANA6"}],

	// None of the counter commands are used  - ACC(12) and ACC24 are not used.
	"Counters" : [	{"Index" : 5, "Group" : 3, "PayloadLocation": "5A","Channel" : 1, "Type":"ACC12"},
					{"Index" : 6, "Group" : 3, "PayloadLocation": "5B","Channel" : 1, "Type":"ACC12"},
					{"Index" : 7, "Group" : 3, "PayloadLocation": "6A","Channel" : 1, "Type":"ACC24"}],

	// CONTROL up to 12 bits per group address, Channel 1 to 12.
	"BinaryControls" : [{"Index": 1,  "Group" : 4, "Channel" : 1, "Type" : "CONTROL"},
                        {"Range" : {"Start" : 10, "Stop" : 21}, "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],

	"AnalogControls" : [{"Index": 1,  "Group" : 3, "Channel" : 1, "Type" : "CONTROL"}],

	// Special definition, so we know where to find the Remote Status Data in the scanned group.
	"RemoteStatus" : [{"Group":3, "Channel" : 1, "PayloadLocation": "7A"}]

}
```



