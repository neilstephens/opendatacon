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
	if(($File::Find::name =~ /\.cpp$|\.h$/i) and not ($File::Find::name =~ /^\.\/submodules|catch\.hpp$|json|_site|^\.\/Release|^\.\/Debug/))
	{	
		print $File::Find::name."\n";
		$cmd = 'perl -0777p -e \'while(s/(#include[^\n]+\n)\n+(#include)/$1$2/mg){pos()-=8}\' '.$File::Find::name.' > '.$File::Find::name.'.incg && mv '.$File::Find::name.'.incg '.$File::Find::name;
		
		$cmd = 'perl -0777p -e \'while(s/(^#include\s+<[^\n]+\n)(^#include\s+"[^\n]+\n)/$2$1/mg){pos()=0}\' '.$File::Find::name.' > '.$File::Find::name.'.incg && mv '.$File::Find::name.'.incg '.$File::Find::name;
		
		if(system($cmd))
		{
			die "failed to uncrustify ($cmd): $!\n";
		}
	}
}

