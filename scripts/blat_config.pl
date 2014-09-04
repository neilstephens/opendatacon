#!/usr/bin/perl
#
#
use strict;
use warnings; 

my $config_header =
'{
	"LogName" :	"Data Concentrator",
	"LogFileSizekB"	:4096,
	"NumLogFiles":	5,
	"LOG_LEVEL":	"Info",
	
	"Ports" :
	[
';
		
my $port_template =
'		{
			"Name" : "InPort_<OUTSTATION_ADDR>",
			"Type" : "DNP3Master",
			"ConfFilename" : "DMCDNP3.conf",
			"ConfOverrides" : { "IP" : "127.0.0.1", "Port" : 30000, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		},
		{
			"Name" : "OutPort_AP_<OUTSTATION_ADDR>",
			"Type" : "DNP3Outstation",
			"ConfFilename" : "DMCDNP3Outstation.conf",
			"ConfOverrides" : { "IP" : "0.0.0.0", "Port" : 20000, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		},
		{
			"Name" : "OutPort_AS_<OUTSTATION_ADDR>",
			"Type" : "DNP3Outstation",
			"ConfFilename" : "DMCDNP3Outstation.conf",
			"ConfOverrides" : { "IP" : "0.0.0.0", "Port" : 20001, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		},
		{
			"Name" : "OutPort_BP_<OUTSTATION_ADDR>",
			"Type" : "DNP3Outstation",
			"ConfFilename" : "DMCDNP3Outstation.conf",
			"ConfOverrides" : { "IP" : "0.0.0.0", "Port" : 20002, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		},
		{
			"Name" : "OutPort_BS_<OUTSTATION_ADDR>",
			"Type" : "DNP3Outstation",
			"ConfFilename" : "DMCDNP3Outstation.conf",
			"ConfOverrides" : { "IP" : "0.0.0.0", "Port" : 20003, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		}
';
		
my $connector_header =
'{
	"Connections" :
	[
';

my $connection_template =
'		{
			"Name" : "Connection_<OUTSTATION_ADDR>a",
			"Port1" : "InPort_<OUTSTATION_ADDR>",
			"Port2" : "OutPort_AP_<OUTSTATION_ADDR>"
		},
		{
			"Name" : "Connection_<OUTSTATION_ADDR>b",
			"Port1" : "InPort_<OUTSTATION_ADDR>",
			"Port2" : "OutPort_AS_<OUTSTATION_ADDR>"
		},
		{
			"Name" : "Connection_<OUTSTATION_ADDR>c",
			"Port1" : "InPort_<OUTSTATION_ADDR>",
			"Port2" : "OutPort_BP_<OUTSTATION_ADDR>"
		},
		{
			"Name" : "Connection_<OUTSTATION_ADDR>d",
			"Port1" : "InPort_<OUTSTATION_ADDR>",
			"Port2" : "OutPort_BS_<OUTSTATION_ADDR>"
		}
';

my $connector_footer =
'	]
}
';
		
my $config_footer =
'	],

	"Connectors" :
	[
		{
			"Name" : "DMC_Connector",
			"ConfFilename" : "DMCConnector.conf"
		}
	]

}
';

open main_conf, ">opendatacon.conf";
open conn_conf, ">DMCConnector.conf";

print main_conf $config_header;
print conn_conf $connector_header;

foreach my $i (1 .. 1000)
{
	my $ports = $port_template;
	$ports =~ s/<OUTSTATION_ADDR>/$i/gs;
	my $connections = $connection_template;
	$connections =~ s/<OUTSTATION_ADDR>/$i/gs;
	
	if($i != 1000)
	{
		$ports = $ports.",";
		$connections = $connections.",";
	}
	
	print main_conf $ports;
	print conn_conf $connections;
}

print main_conf $config_footer;
print conn_conf $connector_footer;

close main_conf;
close conn_conf;
