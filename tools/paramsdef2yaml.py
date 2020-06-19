#! /usr/bin/env python
# 
# A basic script to read in a file in the params.def format used by GCC and
# output optsearch compatible YAML.
#
# Note that there are some problems with this simplistic approach:
# There are mutually exclusive params and flags, and some which have
# dependencies on one another being set, or being set to particular values.
# None of this is represented in the params.def file.
#
import argparse
import ast
import yaml
import collections
import logging
import os
import re

TEST_FILE = './src/helloworld.c'
INPUT_FILE = './params.def'
OUTPUT_FILE = './params.yml'

log = logging.basicConfig(filename='paramsdef2yaml.log', level=logging.INFO)

# TODO Take arguments passed on command line for input and output files
argparser = argparse.ArgumentParser()
argparser.add_argument('--input', help='input file to read from (params.def from GCC)')
argparser.add_argument('--output', help='output file to write out yaml to')

def find_param_range(name, param_min, param_max):
    # TODO Find the real range of this parameter
    # I am going to assume that min is correct, as it usually is.
    # We need to compiler the TEST_FILE using one param at a time and do a
    # fairly simple binary search to find the actual range.
    cmd = "gcc --param=%s=%d %s".format(name, value, TEST_FILE)
    print "Executing command: %s".format(cmd)
    retvalue = os.system(cmd)
    print retvalue
    return param_max

def extract_param_defaults():
    """
    Get the default, minimum, and maximum for each compiler parameter.
    Requires source code for compiler to be in your home directory.
    This example ships with a cached version so it does not require source.
    """
    if os.path.isfile(OUTPUT_FILE) and not args.no_cached_flags:
      # use cached version
      param_defaults = json.load(open(OUTPUT_FILE))
    else:
      # TODO
      # default values of params need to be extracted from source code,
      # since they are not in --help. Later we'll get them from a config file,
      # as other compilers are even less helpful than GCC and are closed
      # source.
      flags = dict()
      param_defaults = []
      flags["flags"] = param_defaults
      params_def = open(os.path.expanduser(INPUT_FILE)).read()
      for m in re.finditer(r'DEFPARAM *\((([^")]|"[^"]*")*)\)', params_def):
        param_def_str = (m.group(1)
                         .replace('GGC_MIN_EXPAND_DEFAULT', '30')
                         .replace('GGC_MIN_HEAPSIZE_DEFAULT', '4096')
                         .replace('50 * 1024 * 1024', '52428800')
                         .replace('INT_MAX', '2147483647'))
        # XXX Also need to replace INT_MAX
        # With 32-bit ints, INT_MAX is 2147483647
        try:
          name, desc, default, param_min, param_max = ast.literal_eval(
              '[' + param_def_str.split(',', 1)[1] + ']')
          # TODO Find the ranges where max == min
          if param_max <= param_min:
              param_max = find_param_range(name, param_min, param_max)
          param_defaults.append({
                                  'name': name,
                                  'type': "range",
                                  'prefix': '--param ',
                                  'separator': '=',
                                  'min': param_min,
                                  'max': param_max
                      })
        except:
          logging.exception("error with %s", param_def_str)
      yaml.dump(flags, open(OUTPUT_FILE, 'w'), default_flow_style=False)
      return

def main():
    args = argparser.parse_args()
    extract_param_defaults()
    return

if __name__ == "__main__":
        main()
