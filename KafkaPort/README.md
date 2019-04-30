## Table of contents

* [Introduction](#introduction)
    * [Features](#features)
		* [What it does do](#What-it-does-do)
		* [What it does not do](#What-it-does-not-do)
	* [Unit Tests](#unit-tests)
	* [Configuration files and syntax](#configuration-files-and-syntax)


## Introduction

KafkaPort is a Port implementation for OpenDataCon that will send data (producer) to a Kafka instance.

## Features

This port only listens for ODC events, and when it receives them, translates the event into a Kafka message, and sends this to the Kafka instance. 
Kafka deals with  Key:Value pairs, sent to "topics". Currently we only send to one topic, and the key is setup in the ODC point definition. 
The value is a JSON string, containing the value and the quality of the point. For example {"Value": 0.123, "Quality": "ONLINE|RESTART"}

Binary and Analog points are supported. The value is a floating point number (for analogs)

### What it does do
This is listen only to the ODC event bus. No events are sent to the ODC event Bus.


## Unit Tests
If you are building under Visual Studio, there is a define (#define NONVSTESTING) that if turned OFF will allow you to have the unit tests appear in the VS Test Explorer.
The version of Catch, catchvs.hpp that allows this to happen is no longer supported, and has some bugs - not all tests will run as a group, but will run individually.
It is very useful in debugging and development!
When this define is turned ON, there is also a separate test program/project that can run all the tests using a "normal" version of catch, by calling a function in the CBPort dll, and you can still run this in VS and set breakpoints etc.


## Configuration files and syntax

Starting with an example configuration:

```json
{
	"IP" : "127.0.0.1",		// Kafka IP Address for initial connection - Kafka then supplies all the IP's of the Kafka cluster. We dont have to worry about this.
	"Port" : 10000,
	"Topic" : "Historian",	// Kafka Topic we will publish messages to. NOTE: we do not specify the partiton (look at the Kafka docs!)


	//-------Point conf--------#
	// Index is the ODC Index. All other fields in the point conf are Kafka fields. 
	// The key will look like: "HS01234|ANA|56" The first part is the EQType, which can be roughly translated as RTU. The second is ANA (analog) or BIN (binary). 
	// The last value is the point number which will be the ODC index. i.e. Set the ODC index to match the point number.
	// The value part of the Kafka pair will be like {"Value": 0.123, "Quality": "|ONLINE|RESTART|"}
	
	"Binaries" : [	{"Range" : {"Start" : 0, "Stop" : 11}, "EQType" : "HS01234" },
					{"Index" : 12, "EQType" : "HS01234"}],

	"Analogs" : [	{"Index" : 0, "EQType" : "HS01234"},
					{"Index" : 1, "EQType" : "HS01234"}]

}
```



