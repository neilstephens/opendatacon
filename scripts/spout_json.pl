#!/usr/bin/perl
#
#
use strict;
use warnings;
use Time::HiRes;
use IO::Socket;

$| = 1;
$SIG{PIPE} = 'IGNORE';

my $json1 = '
{
    // Default encoding for text
    "encoding" : "UTF-8",

    // Plug-ins loaded at start-up
    "plug-ins" : [
        "python",';my $json2 = '
        "c++",
        "ruby"
        ],

    // Tab indent size
    "indent" : { "length" : 3, "use_space": true }
}
';

my $json3 = '
{
	"Weather Data" : {"Wind Speed" : 23, "Humidity" : 10.4},
	"Device" : {"Status" : "GOOD", "Online" : "NO"}
}';
my $json4 = '
{
	"Weather Data" : {"Wind Speed" : 18.6, "Humidity" : 99},
	"Device" : {"Status" : "X", "Online" : "BLAH"}
}';

my $sock = new IO::Socket::INET (
                                  LocalHost => '0.0.0.0',
                                  LocalPort => '2598',
                                  Proto => 'tcp',
                                  Listen => 1,
                                  Reuse => 1,
                                 );
die "Could not create socket: $!\n" unless $sock;

while(1)
{
	print "server waiting for client connection on port 2598\n";
	# waiting for a new client connection
	my $client_socket = $sock->accept();

	# get information about a newly connected client
	my $client_address = $client_socket->peerhost();
	my $client_port = $client_socket->peerport();
	print "connection from $client_address:$client_port\n";

	while(1)
	{
		Time::HiRes::usleep(100000);
		unless(print $client_socket $json3)
		{
			last;
		}
		Time::HiRes::usleep(100000);
		unless(print $client_socket $json4)
		{
			last;
		}
	}
	print "socket disconnected\n";
}

close($sock);
