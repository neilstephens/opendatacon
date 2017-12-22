#!/usr/bin/perl
#
#
use strict;
use warnings;
use File::Find;

find({wanted => \&wanted, no_chdir => 1}, ("./"));

sub wanted
{
	my $cmd;
	if(-d $File::Find::name)
	{
		return;
	}
	if(($File::Find::name =~ /\.cpp$|\.h$/i) and not ($File::Find::name =~ /catch\.hpp$|json|tinycon|libmodbus|^\.\/Release|^\.\/Debug/))
	{
		$cmd = "uncrustify -c uncrustify.cfg --replace --no-backup ".$File::Find::name;
		if(system($cmd))
		{
			die "failed to uncrustify ($cmd): $!\n";
		}
	}
}

