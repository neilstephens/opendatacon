use strict;
use warnings;
use File::Find::Rule;

# find all the subdirectories of a given directory
my @subdirs = File::Find::Rule->directory->in("./");

open MAKELOG, ">make.log";

my %subdirs_hash;
foreach my $dir (@subdirs)
{
	if($dir =~ /(Debug|Release)(-rpi|-RHEL65)?$/)
	{
		my $out = `(cd $dir && echo "$dir" && date && make 2>&1)`;
		print MAKELOG $out;
		if(${^CHILD_ERROR_NATIVE})
		{
			print $out;
		}
	}
}

