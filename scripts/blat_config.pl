#!/usr/bin/perl
#
#
use strict;
use warnings; 

my $config_header =
'{
	"LogName" :	"ODC_Log",
	"LogFileSizekB"	: 50000,
	"NumLogFiles":	1,
	"LOG_LEVEL":	"NORMAL",
	
	"Ports" :
	[
';
		
my $port_template =
'		{
			"Name" : "InPort_<OUTSTATION_ADDR>",
			"Type" : "DNP3Master",
			"Library" : "DNP3Port",
			"ConfFilename" : "DMCDNP3_Master.conf",
			"ConfOverrides" : { "IP" : "127.0.0.1", "Port" : <INPORT>, "MasterAddr" : 0, "OutstationAddr" : 1}
		},
		{
			"Name" : "OutPort_<OUTSTATION_ADDR>",
			"Type" : "DNP3Outstation",
			"Library" : "DNP3Port",
			"ConfFilename" : "DMCDNP3_Outstation.conf",
			"ConfOverrides" : { "IP" : "0.0.0.0", "Port" : <PORT>, "MasterAddr" : 0, "OutstationAddr" : <OUTSTATION_ADDR>}
		}
';
		
my $connector_header =
'{
	"Connections" :
	[
';

my $connection_template =
'		{
			"Name" : "Connection_<OUTSTATION_ADDR>",
			"Port1" : "InPort_<OUTSTATION_ADDR>",
			"Port2" : "OutPort_<OUTSTATION_ADDR>"
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

my $in_port = 30001;
foreach my $i (1 .. 8000)
{
	my $listen_port;
	
	if(($i-1)%8 < 3)
	{
		$listen_port = (((int(($i-1)/8)*3)+(($i-1)%8))%100)+20001;
	}
	elsif(($i-1)%8 < 6)
	{
		$listen_port = (((int(($i-1)/8)*3)+((($i-1)%8)-3))%100)+20101;
	}
	elsif(($i-1)%8 == 6)
	{
		$listen_port = (int(($i-1)/8)%100)+20201;
	}
	elsif(($i-1)%8 == 7)
	{
		$listen_port = (int(($i-1)/8)%100)+20301;
	}
	
	my $ports = $port_template;
	$ports =~ s/<OUTSTATION_ADDR>/$i/gs;
	$ports =~ s/<PORT>/$listen_port/gs;
	$ports =~ s/<INPORT>/$in_port/gs;
	
	$in_port++;
	
	my $connections = $connection_template;
	$connections =~ s/<OUTSTATION_ADDR>/$i/gs;
	
	if($i != 8000)
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
