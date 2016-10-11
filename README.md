## Table of contents

* [Introduction](#introduction)
    * [Core features](#core-features)
    * [Building and Installing](#build-install)
    * [Basic components](#basic-components)
      * [Ports](#ports)
      * [Connectors](#connectors)
        * [Connections](#connections)
        * [Transforms](#transforms)
      * [Extensibility](#extensibility)
    * [Internal data structure](#internal-data-structure)
    * [Configuration files and syntax](#configuration-files-and-syntax)
      * [Main configuration](#main-configuration)
        * [Keys](#keys)
      * [Port configuration](#port-configuration)
        * [Keys](#keys-1)
      * [Connector configuration](#connector-configuration)
        * [Keys](#keys-2)
        * [Config file keys](#config-file-keys)
      * [Connection configuration](#connection-configuration)
        * [Keys](#keys-3)
      * [Transform configuration](#transform-configuration)
        * [Keys](#keys-4)
          * [IndexOffset "Parameters"](#indexoffset-parameters)
          * [Threshold "Parameters"](#threshold-parameters)
          * [Rand "Parameters"](#rand-parameters)
    * [Extensions](#extensions)
      * [DNP3 Port Library](#dnp3-port-library)
        * [Features](#features)
        * [Configuration](#configuration)
          * [DNP3 Master](#dnp3-master)
            * [Example](#example)
            * [Config file keys](#config-file-keys-1)
          * [DNP3 Outstation](#dnp3-outstation)
            * [Example](#example-1)
            * [Config file keys](#config-file-keys-2)
          * [Common keys to DNP3 Master and Outstation](#common-keys-to-dnp3-master-and-outstation)
      * [JSON Port Library](#json-port-library)
        * [Features](#features-1)
        * [Configuration](#configuration-1)
          * [JSON Client](#json-client)
            * [Example](#example-2)
            * [Config file keys](#config-file-keys-3)
    * [API](#api)
      * [Port](#port)
      * [Transform](#transform)
      * [User interface](#user-interface)

## Introduction

opendatacon is designed to be a flexible, highly configurable, modular application. It isn't intended to solve just a singular problem, but rather, provide a tool that can be configured and extended to solve a multitude of problems. This manual will explain core features, inbuilt features and packaged extensions.

## Core features

The minimal requirement of a data concentrator is to connect multiple data channels to single destination. opendatacon meets this requirement, but in a more generic way. It extends the definition to not only data 'concentration' but arbitrary data routing.

*   Combine or pass through any number (within operating system limits) of data interfaces (channels) to any number of other data interfaces. These interfaces are called ports in the context of opendatacon.
*   Ports translate from/to external protocols.
*   Data can be manipulated en-route. These manipulations are called transforms in the context of opendatacon.

These core features are abstract by design, to allow for extension. Some extensions are included in opendatacon, the features of which will be covered in corresponding sections of this manual.

The only real constraint to whether a particular data stream can be handled by opendatacon, is whether the data can be meaningfully 'wrapped' or translated to the internal data structure used by opendatacon. This is discussed further in the internal data structure section. Suffice to say that most SCADA data, irrespective of protocol, should not pose a problem due to the common nature of SCADA applications, and it's difficult to conceive of a data stream that couldn't be wrapped.

## Building and Installing

opendatacon uses the CMake build system.

### Build system requirements

*   Modern C++11 compiler (currently developed under msvc, g++ and clang)
*   for unix environments libstdc > ???

### Dependencies

Core dependencies include:

*   automatak opendnp3: https://www.automatak.com/opendnp3/
*   ASIO: http://think-async.com/
*   TCLAP: http://tclap.sourceforge.net/

Plugin specific dependencies:

*   WebUI: libmicrohttpd https://www.gnu.org/software/libmicrohttpd/
*   ModbusPort: libmodbus http://libmodbus.org/

### CMake options

#### Optional components

*   TEST
*   WEBUI
*   DNP3PORT
*   JSONPORT
*   MODBUSPORT
*   FULL

#### Dependency search locations

*   DNP3_HOME
*   ASIO_HOME
*   TCLAP_HOME
*   MODBUS_HOME
*   MICROHTTPD_HOME   

#### Cross-compiler options

The variable MY_CONFIG_POSTFIX can be set when running cmake to specify the postfix to append to lib search folders.
For example, setting "-DMY_CONFIG_POSTFIX=-rpi" causes the build system to search in "lib-rpi" in the home directory of each dependency.

### Example setup

The following examples all create out-of-source build systems which is recommended.

#### MacOS build and install using Xcode
```
cmake . "-Bbuild-xcode" -G "Xcode" "-DFULL=ON"
```

#### Visual Studio build and install
The following assumes that dependencies have been build and installed in c:\local accordingly.
```
cmake . "-Bbuild-win32" -G "Visual Studio 12 2013" "-DFULL=ON" "-DDNP3_HOME=c:\local\dnp3" "-DASIO_HOME=C:\local\asio-1.10.1" "-DTCLAP_HOME=c:\local\tclap" "-DMICROHTTPD_HOME=c:\local\microhttpd"
```

#### Ubuntu build and install using Makefiles

1.  Install build system and dependencies:
```
sudo apt-get install libasio-dev libtclap-dev libmicrohttpd-dev libmodbus-dev
```
You will also need to build and install opendnp3.
2.  Create build system for desired environment
```
cmake . "-Bbuild-make" -G "Unix Makefiles" "-DFULL=ON"
```
3.  Build
```
cd build-make
make -j 4
```
4.  Install
```
sudo make install
```

## Basic components

Here is an example diagram of basic components for configuring opendatacon, and how they fit together.

![](https://github.com/neilstephens/wikiimages/blob/master/opendatacon_components.png)

### Ports

Ports are the interface between opendatacon and the outside world. It is a Port's job to translate from/to the internal data structures used by opendatacon to/from an external protocol. For example, as of opendatacon 0.3.0, there are built in Port types:

*   DNP3 Master Port
*   DNP3 Outstation Port
*   JSON Client Port

### Connectors

Connectors are simply a 'container' structure for logically delineating groupings of connections and transforms. They are used to add greater flexibility and permutations to how things can be configured. For example, a largely similar pattern of connections/transforms can be easily duplicated using a common connector configuration and only overriding key attributes. More on that in the details of the configuration syntax...

#### Connections

Connections simply open a bi-directional data path between two ports, by subscribing to two ports and routing data between them.

#### Transforms

Transforms filter and transform data on entry to a Connector (ie. they're uni-directional), for a single source of data (sending port). They can be thought of as a subscription filter. For example, a transform could be implemented to add timestamps to untimestamped data, or deadband or throttle a data stream.

### Extensibility

The basic components, described above, form the basis of an extensible platform (read API). Specific Ports and Transforms can be developed to interface/wrap arbitrary protocols and do arbitrary manipulation. And more generally, plugins can be developed, eg. user interface plugins like the included WebUI (more on this later). See the API section of this document for more details.

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

The included components of opendatacon use the data format [JSON](http://json.org/) (JavaScript Object Notation) for configuration. opendatacon doesn't utilise Java or JavaScript in any other capacity, but JSON was chosen for configuration because this usage matches the intention of the format:

_"JSON (JavaScript Object Notation) is a lightweight data-interchange format. It is easy for humans to read and write. It is easy for machines to parse and generate."_ - json.org

In general, the configuration of a component (port, connector, transform, connection) is contained in a JSON object. A JSON object is just a group of key value pairs, where values can be nested objects (or arrays - arrays are just lists of values). These component objects are defined within an overall JSON object. In short - a simple hierarchical configuration, explained below.

For details on JSON syntax, see the handy diagrams at [json.org](http://json.org/). When a value, string, array, object or number is referred to in the following configuration sections, it is as defined in JSON.

### Main configuration

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

#### Keys

Here are the available keys for configuration of the main opendatacon configuration object:

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Ports" | <span>array</span> | a list of port configurations | No, but opendatacon won't do much without any ports | Empty |
| "Connectors" | array | a list of connector configurations | <span>No, but opendatacon won't do much without any connectors</span> | <span>Empty</span> |
| "Plugins" | <span>array</span> | a list of plug-in configurations | No | <span>Empty</span> |
| "LogName" | string | filepath/name prefix for log message files. A number and .txt file extension will be appended | No | "datacon_log" |
| "NumLogFiles" | number | A non-zero number, denoting the number of log files to be used as a 'rolling buffer' of logs. Eg. If 3 is given, files LogName0.txt, <span>LogName1.txt, <span>LogName2.txt will be written to in sequential modulo 3 order.</span></span> | No | 5 |
| "LogFileSizekB" | number | The size in kilobytes after which a log file is full, and the logging system will start a new log file. | No | 5120 |
| "LOG_LEVEL" | string | Either "NOTHING", "NORMAL", "ALL_COMMS", or "ALL". This defines the verbosity of the log messages generated. This corresponds directly with the log levels used by the open dnp3 library, since the DNP3 port implementations are the primary usage of opendatacon as of 0.3.0 | No | "NORMAL" |

### Port configuration

#### Keys

Here are the available keys for configuration of a port in opendatacon. An object with these keys is used as a member of the "Ports" list in the main configuration.


| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Name" | string | The name of the port. This needs to be a unique identifier. | Yes | N/A |
| "Type" | string | This defines the specific implementation of a port. There is an inbuilt port implementation called "Null" (which throws away data - for testing), but otherwise, ports are implemented in libraries, and this is used to find the port construction routine in the library. See "Library" below for how the library itself is found. | Yes | N/A |
| "ConfFilename" | string | The filepath/name to a file containing the implementation specific configuration for the port. This is discussed separately for the included port types in following sections. | Yes | N/A |
| "Library" | string | The base name of the library containing the port implementation. This is required if the library contains multiple port implementations, and hence can't be derived from the port type. Eg. The DNP3 port library contains the port implementations DNP3Outstation and DNP3Master, but the library base name is "DNP3Port" (which resolves to libDNP3Port.so/dylib under POSIX and DNP3Port.dll under windows). By default the library base name is assumed to be "Type"Port. | No | Derived from "Type" |

### Connector configuration

#### Keys

Here are the available keys for configuration of a connector in opendatacon. An object with these keys is used as a member of the "Connectors" list in the main configuration.

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Name" | string | The name of the connector. <span>This needs to be a unique identifier.</span> | Yes | N/A |
| "ConfFilename" | string | <span>The filepath/name to a file containing the JSON object for configuring the connections and transforms belonging to a connector.</span> | Yes | N/A |

Here is an example of the object in the file referred to by "ConfFilename":

```json
{
	"Connections" :
	[
		{
			"Name" : "Test JSON to DNP3",
			"Port1" : "Test JSON input",
			"Port2" : "Test DNP3 output 1"
		},
		{
			"Name" : "Test DNP3 to Null",
			"Port1" : "Test DNP3 Master",
			"Port2" : "Test DNP3 output 2"
		}
	],


	"Transforms" :
	[
		{
			"Type" : "IndexOffset",
			"Sender": "Test JSON input",
			"Parameters" : {
				"Offset" : 1
			}
		}
	]
}
```

#### Config file keys

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Connections"| array | list of connection configurations for the connector | No, but the connector won't do anything without any connections | Empty |
|"Transforms" | array | list of transform configurations for the connector | No | Empty |

### Connection configuration

#### Keys

Here are the available keys for configuration of a connection in opendatacon. An object with these keys is used as a member of the "Connections" list in the connector config file.

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Name" | string | <span>The name of the connection. This needs to be a unique identifier.</span> | Yes | N/A |
| "Port1" and <span>"Port2"</span> | string | The names of the ports that the connection routes between. Notice that there isn't a 'from' or 'to' port, because a connection is bidirectional. | Yes | N/A |

### Transform configuration

#### Keys

Here are the available keys for configuration of a transform in opendatacon. An object with these keys is used as a member of the "Transforms" list in the connector config file.

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Type" | string | This defines the specific implementation of transform to use. As of opendatacon 0.3.0, the inbuilt transforms are "IndexOffset", "Threshold" and "Rand". Transforms will be fully extensible, in the fashion ports are - through a dynamic library API, in subsequent releases of opendatacon. | Yes | N/A |
| "Sender" | string | This should be set to the name of the port that the transform applies to. Any connections in the same connector as the transform, will route data from the specified sender to the transform before routing to the opposite port. | Yes | N/A |
| "Parameters" | value | JSON value to pass to the transform for implementation specific configuration. |
| "Library" | string | The base name of the library containing the <span>transform</span> implementation. This is required if the library contains multiple <span>transform</span> implementations, and hence can't be derived from the <span>transform</span> type. Eg. By default the library base name is assumed to be "Type"Transform. | No | Derived from "Type" |


##### IndexOffset "Parameters"

The IndexOffset transform expects a JSON object with the following keys to be provided as the "Parameters" value:

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "Offset" | number | An integer to add to the point index of a data event. Note this can be negative, but the resulting index should be designed to be unsigned. | No | 0 |


##### Threshold "Parameters"

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "threshold_point_index" | number | The index of the Analog point to be used as the value to threshold gate a group of points | No | 0 |
| "threshold" | number | The value which the Analog value has to meet or surpass to allow data past for the group of points | No | approx -1.79769313486231570815e+308 |
| "points" | <span>array</span> | The point indexes of the points that should be blocked if the Analog value doesn't meet the threshold | No | Empty |


##### Rand "Parameters"

There are no expected parameters for the Rand transform. Any given will be ignored.

## Extensions

### DNP3 Port Library

#### Features

#### Configuration

A DNP3 port is configured by setting the "Type" of a port to either "DNP3Master" or "DNP3Outstation", the "Library" to "DNP3Port", and the "ConfFilename" to a file containing the JSON object discussed below.

##### DNP3 Master

###### Example
```json
{
    //------- DNP3 Link Configuration--------#
    "IP" : 1.2.3.4,
    "Port" : 20000,
    "MasterAddr" : 0,
    "OutstationAddr" : 1,
    "LinkNumRetry" : 0,
    "LinkTimeoutms" : 1000,
    "LinkUseConfirms" : false,
     
    //-------- DNP3 Common Application Configuration -------------#
    "EnableUnsol": true,
    "UnsolClass1": true,
    "UnsolClass2": true,
    "UnsolClass3": true,
    //-------Master Stack conf--------#
    "MasterResponseTimeoutms" : 30000,
    "MasterRespondTimeSync" : true,
    "DoUnsolOnStartup" : true,
    "StartupIntegrityClass0" : true,
    "StartupIntegrityClass1" : true,
    "StartupIntegrityClass2" : true,
    "StartupIntegrityClass3" : true,
    "IntegrityOnEventOverflowIIN" : true,
    "TaskRetryPeriodms" : 30000,
    "IntegrityScanRatems" : 3600000,
    "EventClass1ScanRatems" : 0,
    "EventClass2ScanRatems" : 0,
    "EventClass3ScanRatems" : 0,
    "OverrideControlCode" : "PULSE",
    "DoAssignClassOnStartup" : false,
     
    //-------Point conf--------#
    "Binaries" : [{"Index": 5}],
    "Analogs" : [{"Range" : {"Start" : 0, "Stop" : 5}}],
    "CommsPoint" : {"Index" : 0, "FailValue" : true}
    //"BinaryControls" : [{"Range" : {"Start" : 0, "Stop" : 5}}],  
}
```

###### Config file keys

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| MasterResponseTimeoutms |
| MasterRespondTimeSync |
| DoUnsolOnStartup |
| StartupIntegrityClass0 |
| StartupIntegrityClass1 |
| StartupIntegrityClass2 |
| StartupIntegrityClass3 |
| IntegrityOnEventOverflowIIN |
| TaskRetryPeriodms |
| IntegrityScanRatems |
| EventClass1ScanRatems |
| EventClass2ScanRatems |
| EventClass3ScanRatems |
| DoAssignClassOnStartup |
| OverrideControlCode |
| CommsPoint |


##### DNP3 Outstation

###### Example

```json
{
    //------- DNP3 Link Configuration--------#
    "IP" : 0.0.0.0,
    "Port" : 20000,
    "MasterAddr" : 0,
    "OutstationAddr" : 1,
    "LinkNumRetry" : 0,
    "LinkTimeoutms" : 1000,
    "LinkUseConfirms" : false,
     
    //-------- DNP3 Common Application Configuration -------------#
    "EnableUnsol": true,
    "UnsolClass1": false,
    "UnsolClass2": false,
    "UnsolClass3": false,
     
    //-------Outstation Stack conf--------#
    "SelectTimeoutms" : 30000,
    "SolConfirmTimeoutms" : 30000,
    "UnsolConfirmTimeoutms" : 30000,
    "EventBinaryResponse": "Group2Var2",
    "EventAnalogResponse": "Group32Var5",
    "EventCounterResponse": "Group22Var1",
    "StaticBinaryResponse": "Group1Var2",
    "StaticAnalogResponse": "Group30Var5",
    "StaticCounterResponse": "Group20Var1",
 
    "DemandCheckPeriodms": 1000,
    "WaitForCommandResponses": false,
    //-------Point conf--------#
    "Binaries" : [{"Index": 0},{"Index": 5}],
    "Analogs" : [{"Range" : {"Start" : 0, "Stop" : 5}}]
//"BinaryControls" : [{"Range" : {"Start" : 0, "Stop" : 5}}]
     
}
```

###### Config file keys

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| MaxControlsPerRequest |
| MaxTxFragSize |
| SelectTimeoutms |
| SolConfirmTimeoutms |
| UnsolConfirmTimeoutms |
| WaitForCommandResponses |
| DemandCheckPeriodms |
| StaticBinaryResponse |
| StaticAnalogResponse |
| StaticCounterResponse |
| EventBinaryResponse |
| EventAnalogResponse |
| EventCounterResponse |
| TimestampOverride |
| MaxBinaryEvents |
| MaxAnalogEvents |
| MaxCounterEvents |

##### Common keys to DNP3 Master and Outstation

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| LinkNumRetry |
| LinkTimeoutms |
| LinkUseConfirms |
| UseConfirms |
| EnableUnsol |
| UnsolClass1 |
| UnsolClass2 |
| UnsolClass3 |
| Analogs |
| Binaries |
| BinaryControls |

### JSON Port Library

#### Features

#### Configuration

A JSON port is configured by setting the "Type" of a port to "JSONClient" ("JSONServer" yet to be implemented), the "Library" to "JSONPort", and the "ConfFilename" to a file containing the JSON object discussed below.

##### JSON Client

###### Example

```json
{
    "JSONPointConf" :
    [
    {
        // The type of points - Analog or Binary
        "PointType" : "Analog",
        "Points" :
        [
            {
              "Index" : 0,
              "StartVal" : 3.46,
              "JSONPath" : ["Weather Data","Wind Speed"]
            },
            {
              "JSONPath" : ["Weather Data","Humidity"],
              "Index" : 1
            }
        ]
    },
 
    {
        "PointType" : "Binary",
        "Points" :
        [
            {
              "Index" : 0,
              "JSONPath" : ["Device","Status"],
              "TrueVal" : "GOOD",
              "StartVal" : "GOOD"
            },
            {
              "JSONPath" : ["Device","Online"],
              "Index" : 1,
              "TrueVal" : "YES",
              "FalseVal" : "NO",
              "StartVal" : "SOMETHING ELSE" //true and false are both defined - another state will produce bad quality
            }
        ]
    }
    ]
}
```

###### Config file keys

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
|JSONPointConf | array | A list of objects containing point configuration for a type of points. | No - but the port won't do anything without some point configurations | Empty |
|JSONPointConf[]:PointType | string | A string denoting which type of internal events the points in this <span style="line-height: 1.4285715;">JSONPointConf member will be coverted to. "Analog" and "Binary" support at 0.3.0, "Control" not implemented yet.</span> | Yes | N/A |
|JSONPointConf[]:Points | array | A list of objects containing the configuration for each of the points in this <span style="line-height: 1.4285715;">JSONPointConf member.</span> | No - but there's no reason to have a <span style="line-height: 1.4285715;">JSONPointConf object with no points</span> | Empty |
|JSONPointConf[]:Points[]:JSONPath | array | The JSON Path (sequence of nested keys) as an array of strings, to map to a point | No - but the point will never update | Empty |
|JSONPointConf[]:Points[]:Index | number | <span>The index of the point to map to</span> | Yes | N/A |
|JSONPointConf[]:Points[]:StartVal | value | An optional value to initialise the point | No | undefined |
|JSONPointConf[]:Points[]:TrueVal | value | For "Binary" <span style="line-height: 1.4285715;">PointType, the value which will parse as true</span> | Yes/No - see default | At least one of TrueVal and FalseVal needs to be defined. If only one is defined, any value other than that will parse to be the opposite state. If both are defined, any value other than those will parse to force the point bad quality (but not change state). |
|JSONPointConf[]:Points[]:FalseVal | value | <span>For "Binary"</span> <span>PointType, the value which will parse as false</span> | <span>Yes/No - see default</span> |

## API

### Port

Custom implementations of ports are supported by allowing dynamic libraries to be loaded at runtime. See the port configuration section of this document for how to configure the loading.

To be successfully loaded, a library only needs to export a single C style construction routine for each type of port it implements, of the form:

```c
extern "C" DataPort* new_PORTNAMEPort(std::string Name, std::string File, const Json::Value Overrides);
```

PORTNAME in the example above is the name used as "Type" in the port configuration. It must return a pointer to a new heap allocated instance of a descendant of C++ class DataPort. The declaration of DataPort is included here for context. The whole class hierarchy for DataPort from the public header files (include directory in the opendatacon repository) is needed in practice.

```C++
class DataPort: public IOHandler, public ConfigParser
{
    public: 
        DataPort(std::string aName, std::string aConfFilename, const Json::Value aConfOverrides):
                IOHandler(aName),
                ConfigParser(aConfFilename, aConfOverrides),
                pConf(nullptr)
        {};

        virtual void Enable()=0; virtual void Disable()=0;
        virtual void BuildOrRebuild(asiodnp3::DNP3Manager&amp; DNP3Mgr, openpal::LogFilters& LOG_LEVEL)=0;
        virtual void ProcessElements(const Json::Value&amp; JSONRoot)=0;
        virtual const Json::Value GetStatistics() const { return Json::Value(); };
        virtual const Json::Value GetCurrentState() const { return Json::Value(); };
    protected:
        std::unique_ptr&lt;DataPortConf&gt; pConf;
};
```

### Transform

### User interface


