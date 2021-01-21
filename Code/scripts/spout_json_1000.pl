#!/usr/bin/perl
#
#
use strict;
use warnings;
use Time::HiRes;
use IO::Socket;
use IO::Select;

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

my @socks = ();
foreach my $port (30001 .. 31000)
{
	push(@socks,new IO::Socket::INET (
                                  LocalHost => '0.0.0.0',
                                  LocalPort => $port,
                                  Proto => 'tcp',
                                  Listen => 1,
                                  Reuse => 1,
                                 ));
}

while(1)
{
	print "server waiting for client connections\n";
	my @client_sockets = ();
	my $read_select  = IO::Select->new();
	my $write_select = IO::Select->new();
	foreach my $sock (@socks)
	{
		$read_select->add($sock);
	}

	while(1)
	{
		my @read = $read_select->can_read();
		foreach my $read (@read)
		{
            ## Handle connect from TCP client
            my $new_tcp = $read->accept();
            $write_select->add($new_tcp);
        }
        
        my @write = $write_select->can_write(); 
        foreach my $write (@write)
        {
			## Make sure the socket is still connected before writing.  Do we also
			## need a SIGPIPE handler somewhere?
			if ($write->connected())
			{
				#Time::HiRes::usleep(10000);
				unless($write->send($json3))
				{
					$write_select->remove($write);
					print "-";
				}
				#Time::HiRes::usleep(10000);
				unless($write->send($json4))
				{
					$write_select->remove($write);
					print "-";
				}
			}
			else
			{
				$write_select->remove($write);
				print "-";
			}
		}
	}
}
foreach my $sock (@socks)
{
	close($sock);
}
