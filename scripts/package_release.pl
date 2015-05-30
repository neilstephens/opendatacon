#!/usr/bin/perl
#
#
use strict;
use warnings; 

open VERSION_FILE, "<../ODC/Version.h" or die "Can't open version file: $!\n";

my $major;
my $minor;
my $micro;
while (my $line = <VERSION_FILE>)
{
	$line =~ /ODC_VERSION_MAJOR ([0-9]+)|ODC_VERSION_MINOR ([0-9]+)|ODC_VERSION_MICRO ([0-9]+)/;
	if(defined $1)
	{
		$major = $1
	}
	elsif(defined $2)
	{
		$minor = $2
	}
	elsif(defined $3)
	{
		$micro = $3
	}
}

(defined $major and defined $minor and defined $micro) or die "Failed to parse version file: $!\n";

my $version = "$major.$minor.$micro";
print "Packaging version $version\n";

my %build_configs;

$build_configs{"Debug"}{"Platform"} = "ubuntu1404";
$build_configs{"Debug"}{"Flavour"} = "-Debug";
$build_configs{"Debug"}{"DNP3Libs"} = "../../dnp3-Debug/.libs/*.so*";
$build_configs{"Debug"}{"STDLibs"} = "/usr/lib/x86_64-linux-gnu/libstdc++.so*";
$build_configs{"Debug"}{"MHLibs"} = "/usr/lib/x86_64-linux-gnu/libmicrohttpd.so*";

$build_configs{"Release"}{"Platform"} = "ubuntu1404";
$build_configs{"Release"}{"Flavour"} = "";
$build_configs{"Release"}{"DNP3Libs"} = "../../dnp3/.libs/*.so*";
$build_configs{"Release"}{"STDLibs"} = "/usr/lib/x86_64-linux-gnu/libstdc++.so*";
$build_configs{"Release"}{"MHLibs"} = "/usr/lib/x86_64-linux-gnu/libmicrohttpd.so*";

$build_configs{"Debug-rpi"}{"Platform"} = "rpi";
$build_configs{"Debug-rpi"}{"Flavour"} = "-Debug";
$build_configs{"Debug-rpi"}{"DNP3Libs"} = "../../dnp3-rpi-Debug/.libs/*.so*";
$build_configs{"Debug-rpi"}{"STDLibs"} = "~/x-tools/armv6-rpi-linux-gnueabi/armv6-rpi-linux-gnueabi/sysroot/lib/libstdc++.so*";
$build_configs{"Debug-rpi"}{"MHLibs"} = "../../microhttpd-rpi/lib/libmicrohttpd.so*";

$build_configs{"Release-rpi"}{"Platform"} = "rpi";
$build_configs{"Release-rpi"}{"Flavour"} = "";
$build_configs{"Release-rpi"}{"DNP3Libs"} = "../../dnp3-rpi/.libs/*.so*";
$build_configs{"Release-rpi"}{"STDLibs"} = "~/x-tools/armv6-rpi-linux-gnueabi/armv6-rpi-linux-gnueabi/sysroot/lib/libstdc++.so*";
$build_configs{"Release-rpi"}{"MHLibs"} = "../../microhttpd-rpi/lib/libmicrohttpd.so*";

$build_configs{"Debug-RHEL65"}{"Platform"} = "RHEL65";
$build_configs{"Debug-RHEL65"}{"Flavour"} = "-Debug";
$build_configs{"Debug-RHEL65"}{"DNP3Libs"} = "../../dnp3-RHEL-Debug/.libs/*.so*";
$build_configs{"Debug-RHEL65"}{"STDLibs"} = "~/x-tools/x86_64-RHEL65-linux-gnu/x86_64-RHEL65-linux-gnu/sysroot/lib/libstdc++.so*";
$build_configs{"Debug-RHEL65"}{"MHLibs"} = "../../microhttpd-RHEL/lib/libmicrohttpd.so*";

$build_configs{"Release-RHEL65"}{"Platform"} = "RHEL65";
$build_configs{"Release-RHEL65"}{"Flavour"} = "";
$build_configs{"Release-RHEL65"}{"DNP3Libs"} = "../../dnp3-RHEL/.libs/*.so*";
$build_configs{"Release-RHEL65"}{"STDLibs"} = "~/x-tools/x86_64-RHEL65-linux-gnu/x86_64-RHEL65-linux-gnu/sysroot/lib/libstdc++.so*";
$build_configs{"Release-RHEL65"}{"MHLibs"} = "../../microhttpd-RHEL/lib/libmicrohttpd.so*";

system("mkdir ../../ODC_builds/$version") and die "Couldn't make build dir: $!\n";

foreach my $build_config_key (keys %build_configs)
{
	print "$build_config_key...\n";
	#make build dir
	my $build_dir = "../../ODC_builds/$version/ODC-$version-".$build_configs{$build_config_key}{"Platform"}.$build_configs{$build_config_key}{"Flavour"};
	system("cp -R ../../ODC-pack-common $build_dir") and die "Failed to make build dir '$build_dir': $!\n";
	
	#copy stdlib
	system("cp -a ".$build_configs{$build_config_key}{"STDLibs"}." $build_dir/libs/") and die "Failed to copy std libs for '$build_dir': $!\n";
	
	#copy microhttpd lib
	system("cp -a ".$build_configs{$build_config_key}{"MHLibs"}." $build_dir/libs/") and die "Failed to copy microhttpd libs for '$build_dir': $!\n";
	
	#copy DNP3 libs
	system("cp -a ".$build_configs{$build_config_key}{"DNP3Libs"}." $build_dir/libs/") and die "Failed to copy DNP3 libs for '$build_dir': $!\n";
	
	#copy opendatacon
	system("cp -a ../$build_config_key/opendatacon $build_dir/") and die "Failed to copy opendatacon exe for '$build_dir': $!\n";

	for my $lib ("ODC","JSON","JSONPort","DNP3Port","WebUI")
	{
		#copy lib
		system("cp -a ../$lib/$build_config_key/lib$lib.so $build_dir/libs/") and die "Failed to copy lib$lib for '$build_dir': $!\n";
	}
	
	#hash files 
	system("(cd $build_dir && md5deep -rl ./ > md5sums.txt)") and die "Failed to md5 sum files: $!\n";
	
	#zip it up
	my $rel_dir = "ODC-$version-".$build_configs{$build_config_key}{"Platform"}.$build_configs{$build_config_key}{"Flavour"};
	system("(cd ../../ODC_builds/$version && tar -caf $rel_dir.tar.gz $rel_dir)") and die "Failed to zip $build_dir: $!\n";
	system("md5sum $build_dir.tar.gz > $build_dir.tar.gz.md5") and die "Failed to md5sum $build_dir.tar.gz: $!\n";
	
}




