#!/usr/bin/perl 
# Read YAML in on stdin, dump YAML out; rely on YAML barfing if there is a syntax error

use strict;
use warnings;

use YAML;

local $/;
my $data = <>;
print YAML::Dump(YAML::Load($data));

