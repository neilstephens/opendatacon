#!/usr/bin/perl

#use RUN to execute ldd - this is used to fool cmake BundleUtilities to use a non-native ldd with sysroot
#takes output of ldd and prepends sysroot if needed 

my $run_cmd = $ENV{'RUN'} or "";
my $sysroot = $ENV{'SYSROOT'} or "";
my $ldd_args = join(' ',@ARGV);

my $ldd_out = `$run_cmd ldd $ldd_args`;
#capture the return value
my $ldd_return = $? >> 8;

$sysroot =~ s/\/$//; #remove trailing slash
#prepend sysroot, unless it's already there
$ldd_out =~ s/(\s=>\s)(\Q$sysroot\E)?(\/)/$1$sysroot$3/gs;

#print the fixed output
print $ldd_out;

#return the same value
exit $ldd_return;

