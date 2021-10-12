
| Branch  | Github Actions Linux/Mac/Windows x86/x64/arm builds |
|---------|-----------------------------------------------------|
| develop |[![CI](https://github.com/neilstephens/opendatacon/actions/workflows/opendatacon.yml/badge.svg?branch=develop)](https://github.com/neilstephens/opendatacon/actions/workflows/opendatacon.yml)|
| master  |[![CI](https://github.com/neilstephens/opendatacon/actions/workflows/opendatacon.yml/badge.svg?branch=master)](https://github.com/neilstephens/opendatacon/actions/workflows/opendatacon.yml) |

## Table of contents

* [Quickstart](#quickstart)
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
    * [Elasticsearch](#elasticsearch)
* [API](#api)
    * [Port](#port)
    * [Transform](#transform)
    * [User interface](#user-interface)

## Quickstart

### Pre-built packages

* Download an installer from the [Releases](https://github.com/neilstephens/opendatacon/releases) page.
    * Look for the following in the package name to choose the right package - (OS)-(Arch)-(OS_ver):
        * OS: Operating System - either Linux, Darwin (for OSX), or Windows
        * Arch: Architecture - processor family, bits and/or extensions - x86/i386, x64/x86_64/AMD64, arm(hf|el) etc.
        * OS_ver: target platform details - this doesn't have to match your system exactly because most dependencies are bundled.
            * el6: for Linux systems with older system libraries (glibc), like RHEL/CentOS/OEL 6
            * rpi: Raspbian linux
            * version numbers for OSX and Windows system/SDK.

* Windows builds require the the [x86 MSVC redistributable](https://aka.ms/vs/15/release/VC_redist.x86.exe) or [x64 MSVC redistributable](https://aka.ms/vs/15/release/VC_redist.x64.exe)

* Run the installer, accept the license conditions, choose where to install

* Optionally install as a windows service (--help on the commandline and look for -i)
* Optionally install the included init.d script (in the init dir where you installed) using your linux distro's tools

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

*   Modern C++14 compiler (currently developed under msvc, g++ and clang)

### Dependencies

Core dependencies are set up as git submodules and the default cmake config will auto download and use:

* ASIO: http://think-async.com/
* TCLAP: https://github.com/eile/tclap
* spdlog: https://github.com/gabime/spdlog

Plugin specific dependencies:

* DNP3Port: automatak opendnp3: https://www.automatak.com/opendnp3/
    * default cmake config will auto download and build
* WebUI: libmicrohttpd https://www.gnu.org/software/libmicrohttpd/
    * available in most linux repos (try install libmicrohttpd-dev or libmicrohttpd-devel)
    * download and build yourself for windows - or search the net for binaries
* ModbusPort: libmodbus http://libmodbus.org/
    * available in most linux repos (try install libmodbus-dev or libmodbus-devel)
    * download and build yourself for windows - or search the net for binaries

### CMake options

#### Optional components

*   FULL - build all the optional components
*   TEST - build the optional test suite
*   WEBUI - build the optional Web user interface plugin
*   CONSOLEUI - build the optional console user interface plugin
*   DNP3PORT - build the optional DNP3 protocol ports
*   JSONPORT - build the optional JSON stream ports
*   MODBUSPORT - build the optional modbus protocol ports
*   SIMPORT - build the optional simulated input/output port
*   MD3PORT - build the optional MD3 protocol ports

#### Other options

*   STATIC_LIBSTDC++ - link in libstdc++ statically
*   PACKAGE_LIBSTDC++ - optionally include libstdc++ shared library in installation package
*   PACKAGE_LIBMODBUS - include in the installation package 
*   PACKAGE_LIBMICROHTTPD - include in the installation package

#### Dependency search locations

If you don't want cmake to download/build for you, or you have manually installed in weird locations:

*   DNP3_HOME
*   ASIO_HOME
*   TCLAP_HOME
*   MODBUS_HOME
*   MICROHTTPD_HOME   

#### Cross-compiler options

Cmake cross compiler toolchain files are supported. See the examples in the root of the source tree.

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
make
```
4.  Install
```
sudo make install
```
5.  Optionally generate an installation package
```
make package
```

## Basic components
Here are some examples utilising the basic components for configuring opendatacon, to show how they fit together.

![one_to_one](https://github.com/neilstephens/wikiimages/raw/master/one_to_one.PNG)
Port 1 and port 2 are subscribed to each other by a single connection.

![one_to_two](https://github.com/neilstephens/wikiimages/raw/master/one_to_two.PNG)
Port 1 and port 2 are subscribed to each other. Port 1 and port 3 are also subscribed. Each has its own connection.

![one_to_one_transform](https://github.com/neilstephens/wikiimages/raw/master/one_to_one_transform.PNG)
Port 1 and port 2 are subscribed to each other by a single connection. There is a unidirectional transform affecting the data sent from port 1 to port 2.

![one_to_two_transform](https://github.com/neilstephens/wikiimages/raw/master/one_to_two_transform.PNG)
Port 1 and port 2 are subscribed to each other. Port 1 and port 3 are also subscribed. There is a unidirectional transform affecting the data sent from port 1 to port 2 and port 3. There is also a unidirectional transform affecting the data sent from port 2 to port 1.

### Ports

Ports are the interface between opendatacon and the outside world. It is a Port's job to translate from/to the internal data structures used by opendatacon to/from an external protocol. For example, as of opendatacon 0.4.0, there are built in Port types:

*   DNP3 Master Port
*   DNP3 Outstation Port
*   Modbus Master Port
*   Modbus Outstation Port
*   Simulation Port
*   JSON Client Port
*   Null port

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

##### IndexMap "Parameters"

The IndexMap transform enables point indexes to be remapped. It expects a JSON object with the following keys to be provided as the "Parameters" value:

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "AnalogMap" | JSON object | A JSON object containing two arrays. | No | Empty |
| "BinaryMap" | JSON object | A JSON object containing two arrays. | No | Empty |
| "ControlMap" | JSON object | A JSON object containing two arrays. | No | Empty |

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| "From" | array | An array containing the point indicies from the sending port. | No | Empty |
| "To" | array | An array containing the new point indicies. These are sequential - From[n] will be changed to To[n], where n is the index of the array. | No | Empty |

The below example maps analog point 100-105 to 0-5 (e.g. index 100 becomes index 0)
```json
{
    "Type" : "IndexMap",
    "Sender": "DNP3Master",
    "Parameters" :
    {
        "AnalogMap" :
        {
            "From":
            [100,101,102,103,104,105],
            "To":
            [0,1,2,3,4,5]
        }
    }
}
```
##### Rand "Parameters"

There are no expected parameters for the Rand transform. Any given will be ignored.

## Extensions

### DNP3 Port Library

#### Features

#### Configuration

A DNP3 port is configured by setting the "Type" of a port to either "DNP3Master" or "DNP3Outstation", the "Library" to "DNP3Port", and the "ConfFilename" to a file containing the JSON object discussed below.

##### Common keys to DNP3 Master and Outstation

| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| IP | IP address | IP address of the server if in client mode (see TCPClientServer). IP address of interface to listen on if in server mode. | No | Client: 127.0.0.1, Server: 0.0.0.0 |
| Port | number | Port number to communicate on. | No | 20000 |
| MasterAddr | number | Master station address | No | 0 |
| OutstationAddr | number | Outstation address | No | 1 |
| LinkNumRetry | number | Number of connection attempts | No | 0 |
| LinkTimeoutms | number | Connection timeout in milliseconds | No | 1000 |
| LinkKeepAlivems | number | Time to keep the connection alive for in milliseconds. | No | 10000 |
| LinkUseConfirms | boolean | Request confirmation that the frame arrived | No | false |
| EnableUnsol | boolean | Enable unsolicited events | No | true |
| UnsolClass1 | boolean | Enable Class 1 unsolicited events | No | false |
| UnsolClass2 | boolean | Enable Class 2 unsolicited events | No | false |
| UnsolClass3 | boolean | Enable Class 3 unsolicited events | No | false |
| Analogs | JSON object | List of indvidual index objects, or a range object containing a start and a stop value. | No | empty |
| Binaries | JSON object | List of indvidual index objects, or a range object containing a start and a stop value. | No | empty |
| BinaryControls | JSON object | List of indvidual index objects, or a range object containing a start and a stop value. | No | empty |
| TCPClientServer | string | Return one of CLIENT, SERVER, DEFAULT. | No | CLIENT |
| ServerType | string | Connection type - ONDEMAND, PERSISTENT, MANUAL. | No | ONDEMAND |
| TimestampOverride | enum | Override the timestamp received - ALWAYS, ZERO, NEVER. When configured to ZERO the timestamp will only be overwritten if the received time == 0. | No | ZERO |

##### DNP3 Master

###### Example
```json
{
    //------- DNP3 Link Configuration--------#
    "IP" : "1.2.3.4",
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
| MasterResponseTimeoutms | number | Application layer response timeout. | No | 5000 |
| MasterRespondTimeSync | boolean | If true, the master will do time syncs when it sees the time IIN bit from the outstation. | No | true |
| DoUnsolOnStartup | boolean | If true, the master will enable unsol on startup. | No | true |
| StartupIntegrityClass0 | boolean | If true, the master will perform a Class 0 scan on startup. | No | true |
| StartupIntegrityClass1 | boolean | If true, the master will perform a Class 1 scan on startup. | No | true |
| StartupIntegrityClass2 | boolean | If true, the master will perform a Class 2 scan on startup. | No | true |
| StartupIntegrityClass3 | boolean | If true, the master will perform a Class 3 scan on startup. | No | true |
| IntegrityOnEventOverflowIIN | boolean | Defines whether an integrity scan will be performed when the EventBufferOverflow IIN is detected. | No | true |
| TaskRetryPeriodms | number | Time delay beforce retrying a failed task. | No | 5000 |
| IntegrityScanRatems | number | Frequency of Class 0 (integrity) scan. | No | 3600000 |
| EventClass1ScanRatems | number | Frequency of Class 1 scan. | No | 1000 |
| EventClass2ScanRatems | number | Frequency of Class 2 scan. | No | 1000 |
| EventClass3ScanRatems | number | Frequency of Class 3 scan. | No | 1000 |
| DoAssignClassOnStartup | boolean | | No | false
| OverrideControlCode | opendnp3 ControlCode | Overrides the control code sent by an upstream master station. | No | Undefined
| CommsPoint | JSON object | JSON object containing the point index and fail value | No | empty


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
| MaxControlsPerRequest | number | The maximum number of controls the outstation will attempt to process from a single APDU. | No | 16 |
| MaxTxFragSize | number (kB) | The maximum fragment size the outstation will use for fragments it sends. | No | 2048 |
| SelectTimeoutms | number | How long the outstation will allow an operate to proceed after a prior select. | No | 10000 |
| SolConfirmTimeoutms | number | Timeout for solicited confirms. | No | 5000 |
| UnsolConfirmTimeoutms | number | Timeout for unsolicited confirms. | No | 5000 |
| WaitForCommandResponses | boolean | When responding to a command, wait for downstream command responses, otherwise returns success. | No | false |
| StaticBinaryResponse | DNP3 data type | Group and variation for static binary data | No | Group1Var1 |
| StaticAnalogResponse | DNP3 data type | Group and variation for static analog data | No | Group30Var5 |
| StaticCounterResponse | DNP3 data type | Group and variation for static counter data | No | Group20Var1 |
| EventBinaryResponse | DNP3 data type | Group and variation for event binary data | No | Group2Var1 |
| EventAnalogResponse | DNP3 data type | Group and variation for event analog data | No | Group32Var5 |
| EventCounterResponse | DNP3 data type | Group and variation for event counter data | No | Group22Var1 |
| MaxBinaryEvents | number | The number of binary events the outstation will buffer before overflowing. | No | 1000 |
| MaxAnalogEvents | number | The number of analog events the outstation will buffer before overflowing. | No | 1000 |
| MaxCounterEvents | number | The number of counter events the outstation will buffer before overflowing. | No | 1000 |

### Modbus Port Library
```
Modbus port placeholder
```

### Simulation Port Library
#### Features
#### Configuration
##### Example
```JSON
{
    //-------Point conf--------#
    "Binaries" : 
    [
        {"Index": 0},{"Index": 1},{"Index": 5},{"Index": 6}
    ],

    "Analogs" : 
    [
        {"Range" : {"Start" : 0, "Stop" : 2}, "StartVal" : 50, "UpdateIntervalms" : 0, "StdDev" : 2},
        {"Range" : {"Start" : 3, "Stop" : 5}, "StartVal" : 230, "UpdateIntervalms" : 0, "StdDev" : 5}
    ],

    "BinaryControls" : 
    [
        {"Index" : 0, "FeedbackBinaries" : [{"Index" : 0, "FeedbackMode" : "PULSE"}]}
    ]
}
```
##### Root level keys
| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| Analogs | JSON object | List of items from the analog keys table. | No | empty |
| Binaries | JSON object | List of items from the binary keys table. | No | empty |
| BinaryControls | JSON object | List of items from the binary control keys table. | No | empty |

##### Analog Keys
| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| Index | number | The point index. | Must include a point or a range. | Empty |
| Range | JSON object | The start and stop point indexes (number). E.g. {"Start" : 0, "Stop" : 5} | Must include a point or a range. | Empty |
| StdDev | number | The standard deviation from the mean (StartVal). Used each time the point updates. | No | Empty |
| UpdateIntervalms | number | How often the point will update. | No | Empty |
| StartVal | number | The start value and mean from which each subsequent point update is calculated. | No | Empty |

##### Binary Keys
| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| Index | number | The point index. | Must include a point or a range. | Empty |
| Range | JSON object | The start and stop point indexes (number). E.g. {"Start" : 0, "Stop" : 5} | Must include a point or a range. | Empty |
| UpdateIntervalms | number | How often the point will update. | No | Empty |
| StartVal | boolean ( 0 \| 1 ) | The binary start value. | No | Empty |

##### Binary Control Keys
| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| Index | number | The point index. | Must include a point or a range. | Empty |
| Range | JSON object | The start and stop point indexes (number). E.g. {"Start" : 0, "Stop" : 5} | Must include a point or a range. | Empty |
| Intervalms | number | The duration for which a pulse is applied. | No | Empty |
| FeedbackBinaries | JSON object | JSON object containing the fields in the next table. | No | Empty |

###### Feedback Binary Keys
| Key | Value Type | Description | Mandatory | Default Value |
|-----|------------|-------------|-----------|---------------|
| Index | number | The point index. | Yes | Empty |
| OnValue | boolean ( 0 \| 1 ) |  |  | Empty |
| OffValue | boolean ( 0 \| 1 ) |  |  | Empty |
| Feedback Mode | number | Feedback mode can be PULSE or LATCH. | No | LATCH |

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

#### Elasticsearch

It is possible to historise data from opendatacon by simply pointing a JSON port at Logstash and Elastic search.

A basic configuration of the logstash.conf

```
input {
  tcp {
    port => 2598
    codec => json
    mode => client
    host => "127.0.0.1"
    type => "opendatacon"
  }
}
output {
  elasticsearch {
    hosts => ["localhost:9200"]
    sniffing => true
    manage_template => false
    index => "opendatacon-"
    document_type => "%{[@metadata][type]}"
  }
}
```

### Null Port Library
The null port is equivalent of /dev/null as a DataPort and can be used for testing purposes. There is no configuration data required. 

The null port is simply created as follows:
```JSON
{
	"Name" : "Null",
	"Type" : "Null",
	"ConfFilename" : "/dev/null"
}
```

## API

### Port

Custom implementations of ports are supported by allowing dynamic libraries to be loaded at runtime. See the port configuration section of this document for how to configure the loading.

To be successfully loaded, a library only needs to export a single C style construction routine for each type of port it implements, of the form:

```c
extern "C" DataPort* new_PORTNAMEPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides);
extern "C" void delete_PORTNAMEPort(DataPort*);
```

PORTNAME in the example above is the name used as "Type" in the port configuration. It must return a pointer to a new heap allocated instance of a descendant of C++ class DataPort. The declaration of DataPort is included here for context. The whole class hierarchy for DataPort from the public header files (include directory in the opendatacon repository) is needed in practice.

```C++
class DataPort: public IOHandler, public ConfigParser
{
    public: 
        DataPort(const std::string& aName, const std::string& aConfFilename, const Json::Value& aConfOverrides):
                IOHandler(aName),
                ConfigParser(aConfFilename, aConfOverrides),
                pConf(nullptr)
        {};

        virtual void Enable()=0; virtual void Disable()=0;
        virtual void Build()=0;
        virtual void ProcessElements(const Json::Value& JSONRoot)=0;
        virtual const Json::Value GetStatistics() const { return Json::Value(); };
        virtual const Json::Value GetCurrentState() const { return Json::Value(); };
    protected:
        std::unique_ptr&lt;DataPortConf&gt; pConf;
};
```

### Transform

Similar to Ports, custom Transforms can be implemented as dynamically loaded modules with specified entry points.

```c
extern "C" Transform* new_SOMETransform(const Json::Value& params);
extern "C" void delete_SOMETransform(Transform* pSometx);
```
The 'newing' function just has to construct a instance of a class derriving from the Transform base class (see below) on the heap, and return a pointer to the object.

The 'deleting' function just has to destroy the specified object.

Here is the Transform class declaration, taken from the public header file directory (include dir installed with opendatacon) of opendatacon, for context:

```C++
class Transform
{
public:
	Transform(const Json::Value& params): params(params){}
	virtual ~Transform(){}

	virtual bool Event(Binary& meas, uint16_t& index) { return true; }
	virtual bool Event(DoubleBitBinary& meas, uint16_t& index) { return true; }
	virtual bool Event(Analog& meas, uint16_t& index) { return true; }
	virtual bool Event(Counter& meas, uint16_t& index) { return true; }
	virtual bool Event(FrozenCounter& meas, uint16_t& index) { return true; }
	virtual bool Event(BinaryOutputStatus& meas, uint16_t& index) { return true; }
	virtual bool Event(AnalogOutputStatus& meas, uint16_t& index) { return true; }
	virtual bool Event(ControlRelayOutputBlock& arCommand, uint16_t index) { return true; }
	virtual bool Event(AnalogOutputInt16& arCommand, uint16_t index) { return true; }
	virtual bool Event(AnalogOutputInt32& arCommand, uint16_t index) { return true; }
	virtual bool Event(AnalogOutputFloat32& arCommand, uint16_t index) { return true; }
	virtual bool Event(AnalogOutputDouble64& arCommand, uint16_t index) { return true; }

	virtual bool Event(BinaryQuality qual, uint16_t index) { return true; }
	virtual bool Event(DoubleBitBinaryQuality qual, uint16_t index) { return true; }
	virtual bool Event(AnalogQuality qual, uint16_t index) { return true; }
	virtual bool Event(CounterQuality qual, uint16_t index) { return true; }
	virtual bool Event(FrozenCounterQuality qual, uint16_t index) { return true; }
	virtual bool Event(BinaryOutputStatusQuality qual, uint16_t index) { return true; }
	virtual bool Event(AnalogOutputStatusQuality qual, uint16_t index) { return true; }

	virtual bool Event(ConnectState state, uint16_t index){ return true; }

	Json::Value params;
};
```


### User interface


