## Table of contents

* [Introduction](#introduction)
    * [Features](#features)
		* [What it does do](#What-it-does-do)
		* [What it does not do](#What-it-does-not-do)
	* [Unit Tests](#unit-tests)
	* [Internal data structure](#internal-data-structure)
	* [Configuration files and syntax](#configuration-files-and-syntax)
	* [CBPort Congig File Keys](#cbport-congig-file-keys)

## Introduction

CBPort is a Connitel/Baker Port implementation for OpenDataCon.

## Features

CBPort implements both a Master and Outstation OpenDataCon port, which talks over a TCP connection to Outstations or Scada Masters that use the Conitel or Baker RTU protocols.
It supports multi-drop configuration for both Master and Outstation using the Station Address field to separate traffic.
The only difference between the protocols is the switching of the Station Address and the Group value. (Handled in the Connection Layer, and Block Method DoBakerConitelSwap())

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
|	'1'	| Test RAM	| Yes | We respond with ALL OK |
|	'2'	| Test PROM	| Yes | We respond with ALL OK |
|	'3'	| Test EPROM	| Yes | We respond with ALL OK |
|	'4'	| Test IO	| Yes | We respond with ALL OK |
|	'5'	| Send Time Updates	| Yes | We respond but dont change our time |
|	'6'	| Spare 1	|
|	'7'	| Spare 2	|
|	'8'	| Retrieve Remote Status Word | We reply with all OK |
|	'9'	| Retrieve Input Circuit Data	| No |
|	'10'	| Time Correction Factor Establishment	| No action, but we reply |
|	'11'	| Repeat Previous Transmission	| Yes |
|	'12'	| Spare 3	| No |
|	'13'	| Set Loop backs	| No |
|	'14'	| Reset RTU Warm	| No |
|	'15'	| Reset RTU Cold	| No |


### What it does not do
The following function codes are NOT supported:

| Function Code | Action |
|---------------|--------|
|	'8'	| Reset RTU	|
|	'13'	| Raise/Lower	|
|	'14'	| Freeze and Scan Accumulator	|
|	'15'	| Freeze, Scan and Reset Accumulator	|


## Unit Tests
If you are building under Visual Studio, there is a define (#define NONVSTESTING) that if turned OFF will allow you to have the unit tests appear in the VS Test Explorer.
The version of Catch, catchvs.hpp that allows this to happen is no longer supported, and has some bugs - not all tests will run as a group, but will run individually.
It is very useful in debugging and development!
When this define is turned ON, there is also a separate test program/project that can run all the tests using a "normal" version of catch, by calling a function in the Port dll, and you can still run this in VS and set breakpoints etc.


## Internal data structure

When discussing the configuration of the basic components, it's necessary to understand the structure of the data that is to be manipulated. The internal data structures of opendatacon were adopted from the data structures in the opendnp3 library, because this library was the basis for the first port implementations. The key concepts are:

*   Data is encapsulated by events
*   Events have three parts:
    *   A measurement or payload of type
        *   Analog - a real number (plus quality flags)
        *   Binary - on/off open/close 1/0 etc (plus quality flags), or
        *   Control - currently only binary or unary controls
    *   An implicit or explicit timestamp
    *   An index or address
*   Ports can send and recieve events, and use the index and type as an address to map data from/to multiple points.

## Configuration files and syntax

Starting with an example configuration:

```json
{
    "LogName" :   "ODC_Log",
    "LogFileSizekB"   : 50000,
    "NumLogFiles":    1,
    "LOG_LEVEL":  "NORMAL",

    "Ports" :
    [
        {
            "Name" : "InPort_23",
            "Type" : "DNP3Master",
            "Library" : "DNP3Port",
            "ConfFilename" : "DMCDNP3_Master.conf",
            "ConfOverrides" : { "IP" : "172.24.128.23", "Port" : 20000, "MasterAddr" : 0, "OutstationAddr" : 1}
        },
        {
            "Name" : "OutPort_23",
            "Type" : "DNP3Outstation",
            "Library" : "DNP3Port",
            "ConfFilename" : "DMCDNP3_Outstation.conf",
            "ConfOverrides" : { "IP" : "0.0.0.0", "Port" : 20000, "MasterAddr" : 0, "OutstationAddr" : 23}
        }
    ],
    "Connectors" :
    [
        {
            "Name" : "DMC_Connector",
            "ConfFilename" : "DMCConnector.conf"
        }
    ]
}
```

The main config consists of global settings, a list of ports, a list of connectors and a list of any plugins. The global attributes are discussed in this section, and the hierarchical configuration of ports, connectors and plug-ins are discussed in following sections.

## CBPort Congig File Keys

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Ports" | <span>array</span> | a list of port configurations | No, but opendatacon won't do much without any ports | Empty |
| "Connectors" | array | a list of connector configurations | <span>No, but opendatacon won't do much without any connectors</span> | <span>Empty</span> |
| "Plugins" | <span>array</span> | a list of plug-in configurations | No | <span>Empty</span> |
| "LogName" | string | filepath/name prefix for log message files. A number and .txt file extension will be appended | No | "datacon_log" |
| "NumLogFiles" | number | A non-zero number, denoting the number of log files to be used as a 'rolling buffer' of logs. Eg. If 3 is given, files LogName0.txt, <span>LogName1.txt, <span>LogName2.txt will be written to in sequential modulo 3 order.</span></span> | No | 5 |
| "LogFileSizekB" | number | The size in kilobytes after which a log file is full, and the logging system will start a new log file. | No | 5120 |
| "LOG_LEVEL" | string | Either "NOTHING", "NORMAL", "ALL_COMMS", or "ALL". This defines the verbosity of the log messages generated. This corresponds directly with the log levels used by the open dnp3 library, since the DNP3 port implementations are the primary usage of opendatacon as of 0.3.0 | No | "NORMAL" |




