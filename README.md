## Table of contents

* [Introduction](#introduction)
    * [Core features](#core-features)
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

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td class="confluenceTd">"Ports"</td>

<td class="confluenceTd"><span>array</span></td>

<td class="confluenceTd">a list of port configurations</td>

<td class="confluenceTd">No, but opendatacon won't do much without any ports</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

<tr>

<td class="confluenceTd">"Connectors"</td>

<td class="confluenceTd">array</td>

<td class="confluenceTd">a list of connector configurations</td>

<td class="confluenceTd"><span>No, but opendatacon won't do much without any connectors</span></td>

<td colspan="1" class="confluenceTd"><span>Empty</span></td>

</tr>

<tr>

<td class="confluenceTd">"Plugins"</td>

<td class="confluenceTd"><span>array</span></td>

<td class="confluenceTd">a list of plug-in configurations</td>

<td class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd"><span>Empty</span></td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"LogName"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">filepath/name prefix for log message files. A number and .txt file extension will be appended</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">"datacon_log"</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"NumLogFiles"</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd">A non-zero number, denoting the number of log files to be used as a 'rolling buffer' of logs. Eg. If 3 is given, files LogName0.txt, <span>LogName1.txt, <span>LogName2.txt will be written to in sequential modulo 3 order.</span></span></td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">5</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"LogFileSizekB"</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd">The size in kilobytes after which a log file is full, and the logging system will start a new log file.</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">5120</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"LOG_LEVEL"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">Either "NOTHING", "NORMAL", "ALL_COMMS", or "ALL". This defines the verbosity of the log messages generated. This corresponds directly with the log levels used by the open dnp3 library, since the DNP3 port implementations are the primary usage of opendatacon as of 0.3.0</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">"NORMAL"</td>

</tr>

</tbody>

</table>

</div>

### Port configuration

#### Keys

Here are the available keys for configuration of a port in opendatacon. An object with these keys is used as a member of the "Ports" list in the main configuration.

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Name"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">The name of the port. This needs to be a unique identifier.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Type"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">This defines the specific implementation of a port. There is an inbuilt port implementation called "Null" (which throws away data - for testing), but otherwise, ports are implemented in libraries, and this is used to find the port construction routine in the library. See "Library" below for how the library itself is found.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"ConfFilename"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">The filepath/name to a file containing the implementation specific configuration for the port. This is discussed separately for the included port types in following sections.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Library"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">The base name of the library containing the port implementation. This is required if the library contains multiple port implementations, and hence can't be derived from the port type. Eg. The DNP3 port library contains the port implementations DNP3Outstation and DNP3Master, but the library base name is "DNP3Port" (which resolves to libDNP3Port.so/dylib under POSIX and DNP3Port.dll under windows). By default the library base name is assumed to be "Type"Port.</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">Derived from "Type"</td>

</tr>

</tbody>

</table>

</div>

### Connector configuration

#### Keys

Here are the available keys for configuration of a connector in opendatacon. An object with these keys is used as a member of the "Connectors" list in the main configuration.

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Name"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">The name of the connector. <span>This needs to be a unique identifier.</span></td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"ConfFilename"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd"><span>The filepath/name to a file containing the JSON object for configuring the connections and transforms belonging to a connector.</span></td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">

N/A

</td>

</tr>

</tbody>

</table>

</div>

Here is an example of the object in the file referred to by "ConfFilename":

<div class="code panel pdl" style="border-width: 1px;">

<div class="codeContent panelContent pdl"><script type="syntaxhighlighter" class="theme: Confluence; brush: java; gutter: false"><![CDATA[{ &quot;Connections&quot; : [ { &quot;Name&quot; : &quot;Test JSON to DNP3&quot;, &quot;Port1&quot; : &quot;Test JSON input&quot;, &quot;Port2&quot; : &quot;Test DNP3 output 1&quot; }, { &quot;Name&quot; : &quot;Test DNP3 to Null&quot;, &quot;Port1&quot; : &quot;Test DNP3 input&quot;, &quot;Port2&quot; : &quot;Test DNP3 output 2&quot; } ], &quot;Transforms&quot; : [ { &quot;Type&quot; : &quot;IndexOffset&quot;, &quot;Sender&quot;: &quot;Test JSON input&quot;, &quot;Parameters&quot; : { &quot;Offset&quot; : 1 } } ] }]]></script></div>

</div>

#### Config file keys

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>"Connections"</pre>

</td>

<td colspan="1" class="confluenceTd">array</td>

<td colspan="1" class="confluenceTd">list of connection configurations for the connector</td>

<td colspan="1" class="confluenceTd">No, but the connector won't do anything without any connections</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>"Transforms"</pre>

</td>

<td colspan="1" class="confluenceTd">array</td>

<td colspan="1" class="confluenceTd">list of transform configurations for the connector</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

</tbody>

</table>

</div>

### Connection configuration

#### Keys

Here are the available keys for configuration of a connection in opendatacon. An object with these keys is used as a member of the "Connections" list in the connector config file.

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Name"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd"><span>The name of the connection. This needs to be a unique identifier.</span></td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Port1" and <span>"Port2"</span></td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">The names of the ports that the connection routes between. Notice that there isn't a 'from' or 'to' port, because a connection is bidirectional.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

</tbody>

</table>

</div>

### Transform configuration

#### Keys

Here are the available keys for configuration of a transform in opendatacon. An object with these keys is used as a member of the "Transforms" list in the connector config file.

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Type"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">This defines the specific implementation of transform to use. As of opendatacon 0.3.0, the inbuilt transforms are "IndexOffset", "Threshold" and "Rand". Transforms will be fully extensible, in the fashion ports are - through a dynamic library API, in subsequent releases of opendatacon.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Sender"</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">This should be set to the name of the port that the transform applies to. Any connections in the same connector as the transform, will route data from the specified sender to the transform before routing to the opposite port.</td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Parameters"</td>

<td colspan="1" class="confluenceTd">value</td>

<td colspan="1" class="confluenceTd">JSON value to pass to the transform for implementation specific configuration.</td>

</tr>

<tr>

<td class="confluenceTd">"Library"</td>

<td class="confluenceTd">string</td>

<td class="confluenceTd">The base name of the library containing the <span>transform</span> implementation. This is required if the library contains multiple <span>transform</span> implementations, and hence can't be derived from the <span>transform</span> type. Eg. By default the library base name is assumed to be "Type"Transform.</td>

<td class="confluenceTd">No</td>

<td class="confluenceTd">Derived from "Type"</td>

</tr>

</tbody>

</table>

</div>

##### IndexOffset "Parameters"

The IndexOffset transform expects a JSON object with the following keys to be provided as the "Parameters" value:

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"Offset"</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd">An integer to add to the point index of a data event. Note this can be negative, but the resulting index should be designed to be unsigned.</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">0</td>

</tr>

</tbody>

</table>

</div>

##### Threshold "Parameters"

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"threshold_point_index"</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd">The index of the Analog point to be used as the value to threshold gate a group of points</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">0</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"threshold"</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd">The value which the Analog value has to meet or surpass to allow data past for the group of points</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">approx -1.79769313486231570815e+308</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">"points"</td>

<td colspan="1" class="confluenceTd"><span>array</span></td>

<td colspan="1" class="confluenceTd">The point indexes of the points that should be blocked if the Analog value doesn't meet the threshold</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

</tbody>

</table>

</div>

##### Rand "Parameters"

There are no expected parameters for the Rand transform. Any given will be ignored.

## Extensions

### <span style="font-size: 16.0px;line-height: 1.5625;">DNP3 Port Library</span>

#### <span style="font-size: 16.0px;line-height: 1.5625;">Features</span>

#### Configuration

A DNP3 port is configured by setting the "Type" of a port to either "DNP3Master" or "DNP3Outstation", the "Library" to "DNP3Port", and the "ConfFilename" to a file containing the JSON object discussed below.

##### DNP3 Master

###### Example

<div class="code panel pdl" style="border-width: 1px;">

<div class="codeContent panelContent pdl"><script type="syntaxhighlighter" class="theme: Confluence; brush: java; gutter: false"><![CDATA[{ //------- DNP3 Link Configuration--------# &quot;IP&quot; : 1.2.3.4, &quot;Port&quot; : 20000, &quot;MasterAddr&quot; : 0, &quot;OutstationAddr&quot; : 1, &quot;LinkNumRetry&quot; : 0, &quot;LinkTimeoutms&quot; : 1000, &quot;LinkUseConfirms&quot; : false, //-------- DNP3 Common Application Configuration -------------# &quot;EnableUnsol&quot;: true, &quot;UnsolClass1&quot;: true, &quot;UnsolClass2&quot;: true, &quot;UnsolClass3&quot;: true, //-------Master Stack conf--------# &quot;MasterResponseTimeoutms&quot; : 30000, &quot;MasterRespondTimeSync&quot; : true, &quot;DoUnsolOnStartup&quot; : true, &quot;StartupIntegrityClass0&quot; : true, &quot;StartupIntegrityClass1&quot; : true, &quot;StartupIntegrityClass2&quot; : true, &quot;StartupIntegrityClass3&quot; : true, &quot;IntegrityOnEventOverflowIIN&quot; : true, &quot;TaskRetryPeriodms&quot; : 30000, &quot;IntegrityScanRatems&quot; : 3600000, &quot;EventClass1ScanRatems&quot; : 0, &quot;EventClass2ScanRatems&quot; : 0, &quot;EventClass3ScanRatems&quot; : 0, &quot;OverrideControlCode&quot; : &quot;PULSE&quot;, &quot;DoAssignClassOnStartup&quot; : false, //-------Point conf--------# &quot;Binaries&quot; : [{&quot;Index&quot;: 5}], &quot;Analogs&quot; : [{&quot;Range&quot; : {&quot;Start&quot; : 0, &quot;Stop&quot; : 5}}], &quot;CommsPoint&quot; : {&quot;Index&quot; : 0, &quot;FailValue&quot; : true} //&quot;BinaryControls&quot; : [{&quot;Range&quot; : {&quot;Start&quot; : 0, &quot;Stop&quot; : 5}}], }]]></script></div>

</div>

###### Config file keys

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MasterResponseTimeoutms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MasterRespondTimeSync</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">DoUnsolOnStartup</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StartupIntegrityClass0</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StartupIntegrityClass1</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StartupIntegrityClass2</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StartupIntegrityClass3</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">IntegrityOnEventOverflowIIN</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">TaskRetryPeriodms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">IntegrityScanRatems</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventClass1ScanRatems</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventClass2ScanRatems</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventClass3ScanRatems</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">DoAssignClassOnStartup</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">OverrideControlCode</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">CommsPoint</td>

</tr>

</tbody>

</table>

</div>

##### DNP3 Outstation

###### Example

<div class="code panel pdl" style="border-width: 1px;">

<div class="codeContent panelContent pdl"><script type="syntaxhighlighter" class="theme: Confluence; brush: java; gutter: false"><![CDATA[{ //------- DNP3 Link Configuration--------# &quot;IP&quot; : 0.0.0.0, &quot;Port&quot; : 20000, &quot;MasterAddr&quot; : 0, &quot;OutstationAddr&quot; : 1, &quot;LinkNumRetry&quot; : 0, &quot;LinkTimeoutms&quot; : 1000, &quot;LinkUseConfirms&quot; : false, //-------- DNP3 Common Application Configuration -------------# &quot;EnableUnsol&quot;: true, &quot;UnsolClass1&quot;: false, &quot;UnsolClass2&quot;: false, &quot;UnsolClass3&quot;: false, //-------Outstation Stack conf--------# &quot;SelectTimeoutms&quot; : 30000, &quot;SolConfirmTimeoutms&quot; : 30000, &quot;UnsolConfirmTimeoutms&quot; : 30000, &quot;EventBinaryResponse&quot;: &quot;Group2Var2&quot;, &quot;EventAnalogResponse&quot;: &quot;Group32Var5&quot;, &quot;EventCounterResponse&quot;: &quot;Group22Var1&quot;, &quot;StaticBinaryResponse&quot;: &quot;Group1Var2&quot;, &quot;StaticAnalogResponse&quot;: &quot;Group30Var5&quot;, &quot;StaticCounterResponse&quot;: &quot;Group20Var1&quot;, &quot;DemandCheckPeriodms&quot;: 1000, &quot;WaitForCommandResponses&quot;: false, //-------Point conf--------# &quot;Binaries&quot; : [{&quot;Index&quot;: 0},{&quot;Index&quot;: 5}], &quot;Analogs&quot; : [{&quot;Range&quot; : {&quot;Start&quot; : 0, &quot;Stop&quot; : 5}}] //&quot;BinaryControls&quot; : [{&quot;Range&quot; : {&quot;Start&quot; : 0, &quot;Stop&quot; : 5}}] }]]></script></div>

</div>

###### Config file keys

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MaxControlsPerRequest</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MaxTxFragSize</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">SelectTimeoutms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">SolConfirmTimeoutms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">UnsolConfirmTimeoutms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">WaitForCommandResponses</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">DemandCheckPeriodms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StaticBinaryResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StaticAnalogResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">StaticCounterResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventBinaryResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventAnalogResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EventCounterResponse</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">TimestampOverride</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MaxBinaryEvents</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MaxAnalogEvents</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">MaxCounterEvents</td>

</tr>

</tbody>

</table>

</div>

##### Common keys to DNP3 Master and Outstation

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">LinkNumRetry</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">LinkTimeoutms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">LinkUseConfirms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">UseConfirms</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">EnableUnsol</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">UnsolClass1</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">UnsolClass2</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">UnsolClass3</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">Analogs</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">Binaries</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">BinaryControls</td>

</tr>

</tbody>

</table>

</div>

### JSON Port Library

#### Features

#### Configuration

A JSON port is configured by setting the "Type" of a port to "JSONClient" ("JSONServer" yet to be implemented), the "Library" to "JSONPort", and the "ConfFilename" to a file containing the JSON object discussed below.

##### JSON Client

###### Example

<div class="code panel pdl" style="border-width: 1px;">

<div class="codeContent panelContent pdl"><script type="syntaxhighlighter" class="theme: Confluence; brush: java; gutter: false"><![CDATA[{ &quot;JSONPointConf&quot; : [ { // The type of points - Analog or Binary &quot;PointType&quot; : &quot;Analog&quot;, &quot;Points&quot; : [ { &quot;Index&quot; : 0, &quot;StartVal&quot; : 3.46, &quot;JSONPath&quot; : [&quot;Weather Data&quot;,&quot;Wind Speed&quot;] }, { &quot;JSONPath&quot; : [&quot;Weather Data&quot;,&quot;Humidity&quot;], &quot;Index&quot; : 1 } ] }, { &quot;PointType&quot; : &quot;Binary&quot;, &quot;Points&quot; : [ { &quot;Index&quot; : 0, &quot;JSONPath&quot; : [&quot;Device&quot;,&quot;Status&quot;], &quot;TrueVal&quot; : &quot;GOOD&quot;, &quot;StartVal&quot; : &quot;GOOD&quot; }, { &quot;JSONPath&quot; : [&quot;Device&quot;,&quot;Online&quot;], &quot;Index&quot; : 1, &quot;TrueVal&quot; : &quot;YES&quot;, &quot;FalseVal&quot; : &quot;NO&quot;, &quot;StartVal&quot; : &quot;SOMETHING ELSE&quot; //true and false are both defined - another state will produce bad quality } ] } ] }]]></script></div>

</div>

###### Config file keys

<div class="table-wrap">

<table class="confluenceTable">

<tbody>

<tr>

<th class="confluenceTh">Key</th>

<th class="confluenceTh">Value Type</th>

<th class="confluenceTh">Description</th>

<th class="confluenceTh">Mandatory</th>

<th colspan="1" class="confluenceTh">Default Value</th>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf</pre>

</td>

<td colspan="1" class="confluenceTd">array</td>

<td colspan="1" class="confluenceTd">A list of objects containing point configuration for a type of points.</td>

<td colspan="1" class="confluenceTd">No - but the port won't do anything without some point configurations</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:PointType</pre>

</td>

<td colspan="1" class="confluenceTd">string</td>

<td colspan="1" class="confluenceTd">A string denoting which type of internal events the points in this <span style="line-height: 1.4285715;">JSONPointConf member will be coverted to. "Analog" and "Binary" support at 0.3.0, "Control" not implemented yet.</span></td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points</pre>

</td>

<td colspan="1" class="confluenceTd">array</td>

<td colspan="1" class="confluenceTd">A list of objects containing the configuration for each of the points in this <span style="line-height: 1.4285715;">JSONPointConf member.</span></td>

<td colspan="1" class="confluenceTd">No - but there's no reason to have a <span style="line-height: 1.4285715;">JSONPointConf object with no points</span></td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points[]:JSONPath</pre>

</td>

<td colspan="1" class="confluenceTd">array</td>

<td colspan="1" class="confluenceTd">The JSON Path (sequence of nested keys) as an array of strings, to map to a point</td>

<td colspan="1" class="confluenceTd">No - but the point will never update</td>

<td colspan="1" class="confluenceTd">Empty</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points[]:Index</pre>

</td>

<td colspan="1" class="confluenceTd">number</td>

<td colspan="1" class="confluenceTd"><span>The index of the point to map to</span></td>

<td colspan="1" class="confluenceTd">Yes</td>

<td colspan="1" class="confluenceTd">N/A</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points[]:StartVal</pre>

</td>

<td colspan="1" class="confluenceTd">value</td>

<td colspan="1" class="confluenceTd">An optional value to initialise the point</td>

<td colspan="1" class="confluenceTd">No</td>

<td colspan="1" class="confluenceTd">undefined</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points[]:TrueVal</pre>

</td>

<td colspan="1" class="confluenceTd">value</td>

<td colspan="1" class="confluenceTd">For "Binary" <span style="line-height: 1.4285715;">PointType, the value which will parse as true</span></td>

<td colspan="1" class="confluenceTd">Yes/No - see default</td>

<td rowspan="2" class="confluenceTd">At least one of TrueVal and FalseVal needs to be defined. If only one is defined, any value other than that will parse to be the opposite state. If both are defined, any value other than those will parse to force the point bad quality (but not change state).</td>

</tr>

<tr>

<td colspan="1" class="confluenceTd">

<pre>JSONPointConf[]:Points[]:FalseVal</pre>

</td>

<td colspan="1" class="confluenceTd">value</td>

<td colspan="1" class="confluenceTd"><span>For "Binary"</span> <span>PointType, the value which will parse as false</span></td>

<td colspan="1" class="confluenceTd"><span>Yes/No - see default</span></td>

</tr>

</tbody>

</table>

</div>

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


