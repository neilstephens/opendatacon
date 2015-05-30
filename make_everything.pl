use strict;
use warnings;
use File::Find::Rule;

# find all the subdirectories of a given directory
my @subdirs = File::Find::Rule->directory->in("./");

open MAKELOG, ">make.log";

my %build_order = 
(
	"JSON" => 0,
	"ODC" => 1,
	"opendatacon" => 2,
	"DNP3Port" => 3,
	"JSONPort" => 4,
	"WebUI" => 5
);
my $projects = join("|",keys %build_order);

my %subdirs_hash;
foreach my $dir (@subdirs)
{
	if(("opendatacon/".$dir) =~ /($projects)\/(Debug|Release)(-rpi|-RHEL65)?$/)
	{
		$subdirs_hash{$dir} = $build_order{$1};
	}
}

foreach my $dir (sort {$subdirs_hash{$a}<=>$subdirs_hash{$b}} (keys %subdirs_hash))
{
	my $out = `(cd $dir && echo "$dir" && date && make 2>&1)`;
	print MAKELOG $out;
	if(${^CHILD_ERROR_NATIVE})
	{
		print $out;
	}
}
