use strict;
use warnings;
use File::Find::Rule;

# find all the subdirectories of a given directory
my @subdirs = File::Find::Rule->directory->in("./");

open MAKELOG, ">make.log";
my $failed = 0;

my %subdirs_hash;
foreach my $dir (@subdirs)
{
	if($dir =~ /(Debug|Release)(-rpi|-RHEL65)?$/)
	{
		my $out = `(cd $dir && echo "\\n\\n--------------------\\n$dir" && date && echo "--------------------\\n" && make 2>&1)`;
		if(${^CHILD_ERROR_NATIVE})
		{
			$failed = 1;
		}
		print $out;
		print MAKELOG $out;
	}
}

if($failed == 1)
{
	print "\n\n\n FAIL !!!!!!!!!!!!!! \n\n\n";
}
else
{
	print "\n\n\n DONE \n\n\n";
}

