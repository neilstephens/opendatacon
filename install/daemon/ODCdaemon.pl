#!/usr/bin/perl
#
#
use strict;
use warnings;
use POSIX "setsid"; #for daemonisation to start new session
use POSIX ":sys_wait_h"; #for handling dead children (waitpid flags)

our $PIDFILE = '/var/run/ODCdaemon.pid';
our $LOGFILE = '/var/log/ODCdaemon.log';
our $ODCPATH = '/opt/ODC';
our $ODCWDIR = '/etc/opendatacon/';
our $ODCCONF = 'opendatacon.conf';

daemonize();

#setup signal handlers, so we exit cleanly and monitor children.
$SIG{TERM} = 'sigHandler';
$SIG{INT} = 'sigHandler';
$SIG{PIPE} = 'sigHandler';
$SIG{ALRM} = 'KeepAlive';
$SIG{CHLD} = 'ChildDied';

our $ODC;
our $ODCpid;
launch_ODC();

# Kick off the keep alive sequence
alarm(30);

# Wake up every now and then to see if we should exit
our $run = 1;
while($run)
{
	sleep 1;
}

# Daemon has been asked to terminate, terminate opendatacon
print localtime().": ODCdaemon: stopping opendatacon\n";
kill 'SIGTERM', $ODCpid;

# Wait up to 30 seconds for process to exit cleanly
for(1 .. 30)
{
	unless(kill 0, $ODCpid)
	{
		print localtime().": ODCdaemon: opendatacon cleanly stopped\n";
		last;
	}
	sleep 1;
}

# If still running, kill process
if(kill 0, $ODCpid)
{
	kill 'SIGKILL', $ODCpid;
	print localtime().": ODCdaemon: opendatacon forcefully killed\n";
}

#based on http://perldoc.perl.org/perlipc.html
sub daemonize {
	chdir("/")                      || die "can't chdir to /: $!";
	open(STDIN,  "< /dev/null")     || die "can't read /dev/null: $!";
	open(STDOUT, ">$LOGFILE") or die "ODCdaemon: Can't open log file: $!";
	open(STDERR, ">&STDOUT") or die "ODCdaemon: can't dup stdout: $!";

	if (open F, "<$PIDFILE")
	{
		my $pid = <F>;
		#check if processes are running
		if(kill 0, $pid)
		{
			die "ODCdaemon already running: $pid";
		}
	}

	defined(my $pid = fork())		|| die "can't fork: $!";
	exit if $pid;                   # non-zero now means I am the parent
	(setsid() != -1)                || die "Can't start a new session: $!";
	open F, ">$PIDFILE";
	print F $$;
	close F;
}
sub launch_ODC
{
	#launch opendatacon
	$ODCpid = open($ODC, "|-", "exec env LD_LIBRARY_PATH=$ODCPATH/libs/ $ODCPATH/opendatacon -p $ODCWDIR -c $ODCCONF > /dev/null") or die "ODCdaemon: can't launch opendatacon: $!";
}
sub KeepAlive
{
	if($run)
	{
		#check if processes are running
		unless(kill 0, $ODCpid)
		{
			print localtime().": ODCdaemon: opendatacon not running... restarting\n";
			launch_ODC();
		}
		alarm(30);
	}
}
sub ChildDied
{
	 while (waitpid(-1, WNOHANG) > 0)
	 {
		 print localtime().": ODCdaemon: child died\n";
	 }
}
sub sigHandler
{
	$run = 0;
	#We're exiting, but not necessarily our children - let the OS handle them from now
	$SIG{CHLD}='IGNORE';
}
