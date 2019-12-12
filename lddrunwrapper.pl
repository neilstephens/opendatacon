#!/usr/bin/perl

#use RUN to execute ldd - this is used to fool cmake BundleUtilities to use a non-native ldd with sysroot
#takes output of ldd and prepends sysroot if needed 

my $run_cmd = $ENV{'RUN'} or "";
my $sysroot = $ENV{'SYSROOT'} or "";
my $src_dir = $ENV{'SRC_DIR'} or "";
my $ldd_args = join(' ',@ARGV);

my $ldd_out = `$run_cmd ldd $ldd_args`;
#capture the return value
my $ldd_return = $? >> 8;

$sysroot =~ s/\/$//; #remove trailing slash
$src_dir =~ s/\/$//; #remove trailing slash

#prepend sysroot if it's a docker local path
my @ldd_lines = split(/^/,$ldd_out);
for my $i (0 .. (scalar @ldd_lines -1))
{
	if(not $ldd_lines[$i] =~ /\s=>\s\Q$src_dir\E/)
	{
		$ldd_lines[$i] =~ s/(\s=>\s)(\/)/$1$sysroot$2/;
	} 
}

#print the fixed output
print @ldd_lines;

#return the same value
exit $ldd_return;

