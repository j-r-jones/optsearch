#!/usr/bin/env python
#
# This is a Python 2.7 script.
#
# This is a hastily put together script that borrows from OpenTuner's source
# as well as the earlier OptSearch prototype.  It is intended to generate the
# compiler section of the config file for OptSearch.  It really needs
# re-implementing in C in a more simple form, so that it can be self-contained
# without dependencies.  Otherwise we won't be able to run it on the target
# hardware.
# As a case in point, this Python script works on Balena but it fails on
# Amber.
#
# Known bug:  This script does not manage to pick up on flags that take one of
# a list of values, so they must be added by hand at present.
#

import sys
import os
import subprocess
import argparse
import re
import ast
import logging
import numpy as np
import signal
import threading
import time
from multiprocessing.pool import ThreadPool

try:
  import resource
except ImportError:
  resource = None

try:
  import fcntl
except ImportError:
  fcntl = None

log = logging.getLogger(__name__)
logging.basicConfig(format='%(asctime)s %(message)s')

the_io_thread_pool = None
pids = []
cc_param_defaults = {}

argparser = argparse.ArgumentParser()
argparser.add_argument('--target-input',
        help='How to get target flags from compiler (eg --help=target)',
        default='--help=target', dest='target_input')
argparser.add_argument('--opt-input',
        help='How to get optimisation flags from compiler (eg --help=optimizers)',
        default='--help=optimizers', dest='opt_input')
argparser.add_argument('-o', '--output', help='output file to write out yaml to',
        default='compiler_flags.yaml')
argparser.add_argument('-c', '--compiler', help='compiler we are using',
        default='gcc')
argparser.add_argument('-s', '--source', help='source file to read in (eg params.def)')
argparser.add_argument('-t', '--test-program',
        help='test source program (eg helloworld) in appropriate language',
        default='../test/helloworld.c', dest='test')
argparser.add_argument('-l', '--compile-limit', type=float, default=5,
        help='kill gcc if it runs more than {default} sec')
argparser.add_argument('-p', '--parallelism', type=int, default=2,
        help='number of threads to use')
argparser.add_argument('-d', '--debug', action='store_true')
args = argparser.parse_args()

def extract_working_target_flags(args):
    """
    Figure out which compiler flags work (don't cause compiler to break) by running
    each one.
    """
    targets, err = subprocess.Popen([args.compiler, args.target_input],
                                       stdout=subprocess.PIPE).communicate()
    found_compiler_flags = re.findall(r'^  (-m[a-z0-9-]+) ', targets,
                                re.MULTILINE)
    log.info('Determining which of %s possible target flags work',
             len(found_compiler_flags))
    found_compiler_flags = filter(check_if_flag_works, found_compiler_flags)
    log.info("Returning %s target flags", len(found_compiler_flags));
    return found_compiler_flags

def extract_working_opt_flags(args):
    """
    Figure out which compiler flags work (don't cause compiler to break) by running
    each one.
    """
    optimizers, err = subprocess.Popen([args.compiler, args.opt_input],
                                       stdout=subprocess.PIPE).communicate()
    found_compiler_flags = re.findall(r'^  (-f[a-z0-9-]+) ', optimizers,
                                re.MULTILINE)
    log.info('Determining which of %s possible optimisation flags work',
             len(found_compiler_flags))
    found_compiler_flags = filter(check_if_flag_works, found_compiler_flags)
    log.info("Returning %s optimisation flags", len(found_compiler_flags));
    return found_compiler_flags

def extract_working_flags(args):
    optflags = extract_working_opt_flags(args)
    targflags = extract_working_target_flags(args)

    return (optflags + targflags)

def extract_compiler_version(args):
    m = re.search(r'([0-9]+)[.]([0-9]+)[.]([0-9]+)', subprocess.check_output([
        args.compiler, '--version']))
    if m:
      compiler_version = tuple(map(int, m.group(1, 2, 3)))
    else:
      compiler_version = None
    log.debug('compiler version %s', compiler_version)
    return compiler_version

def the_io_thread_pool_init(parallelism=1):
  global the_io_thread_pool
  if the_io_thread_pool is None:
    the_io_thread_pool = ThreadPool(2 * parallelism)
    # make sure the threads are started up
    the_io_thread_pool.map(int, range(2 * parallelism))

def call_program(args, cmd, limit=None, memory_limit=None, **kwargs):
    """
    call cmd and kill it if it runs for longer than limit

    returns dictionary like
      {'returncode': 0,
       'stdout': '', 'stderr': '',
       'timeout': False, 'time': 1.89}
    """
    the_io_thread_pool_init(args.parallelism)
    if limit is float('inf'):
      limit = None
    if type(cmd) in (str, unicode):
      kwargs['shell'] = True
    killed = False
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         preexec_fn=preexec_setpgid_setrlimit(memory_limit),
                         **kwargs)
    # Add p.pid to list of processes to kill in case of keyboardinterrupt
    #pid_lock.acquire()
    pids.append(p.pid)
    #pid_lock.release()

    try:
      stdout_result = the_io_thread_pool.apply_async(p.stdout.read)
      stderr_result = the_io_thread_pool.apply_async(p.stderr.read)
      while p.returncode is None:
        if limit is None:
          goodwait(p)
        elif limit and time.time() > t0 + limit:
          killed = True
          goodkillpg(p.pid)
          goodwait(p)
        else:
          # still waiting...
          sleep_for = limit - (time.time() - t0)
          if not stdout_result.ready():
            stdout_result.wait(sleep_for)
          elif not stderr_result.ready():
            stderr_result.wait(sleep_for)
          else:
            #TODO(jansel): replace this with a portable waitpid
            time.sleep(0.001)
        p.poll()
    except:
      if p.returncode is None:
        goodkillpg(p.pid)
      raise
    finally:
      # No longer need to kill p
      #lf.pid_lock.acquire()
      if p.pid in pids:
        pids.remove(p.pid)
      #self.pid_lock.release()

    t1 = time.time()
    t = float('inf') if killed else (t1 - t0)

    result = {'time': t,
            'timeout': killed,
            'returncode': p.returncode,
            'stdout': stdout_result.get(),
            'stderr': stderr_result.get()}
    #log.debug(result)
    return result

def goodkillpg(pid):
  """
  wrapper around kill to catch errors
  """
  log.debug("killing pid %d", pid)
  try:
    if hasattr(os, 'killpg'):
      os.killpg(pid, signal.SIGKILL)
    else:
      os.kill(pid, signal.SIGKILL)
  except:
    log.error('error killing process %s', pid, exc_info=True)

def goodwait(p):
  """
  python doesn't check if its system calls return EINTR, retry if it does
  """
  while True:
    try:
      rv = p.wait()
      return rv
    except OSError, e:
      if e.errno != errno.EINTR:
        raise

def check_if_flag_works(flag, try_inverted=True):
    log.debug("Flag is: {0}".format(flag))
    cmd = "{0} {1} {2}".format(args.compiler, flag, args.test)
    log.debug("Running command: {0}".format(cmd))
    compile_result = call_program(args, cmd, limit=args.compile_limit)
    if compile_result['returncode'] != 0:
      log.warning("removing flag %s because it results in compile error", flag)
      return False
    if 'warning: this target' in compile_result['stderr']:
      log.warning("removing flag %s because not supported by target", flag)
      return False
    if 'has been renamed' in compile_result['stderr']:
      log.warning("removing flag %s because renamed", flag)
      return False
    if try_inverted and flag[:2] == '-f':
      if not check_if_flag_works(invert_compiler_flag(flag),
                                      try_inverted=False):
        log.warning("Odd... %s works but %s does not", flag,
                    invert_compiler_flag(flag))
        return False
    return True

def invert_compiler_flag(flag):
  assert flag[:2] == '-f'
  if flag[2:5] != 'no-':
    return '-fno-' + flag[2:]
  return '-f' + flag[5:]

def parse_params(args, cc_param_defaults):
    params, err = subprocess.Popen(
        [args.compiler, '--help=params'], stdout=subprocess.PIPE).communicate()
    all_params = re.findall(r'^  ([a-z0-9-]+) ', params, re.MULTILINE)

    all_params = sorted(set(all_params) &
            set(cc_param_defaults.keys()))

    log.info('Determining which of %s possible gcc params work',
           len(all_params))
    working_params = []
    for param in all_params:
        if check_if_flag_works('--param={0}={1}'.format(
                param, cc_param_defaults[param]['default'])):
          working_params.append(param)
    return working_params

def find_valid_flag_max(flag, prefix, separator, param_min):
    """
    Binary search for valid max value
    """
    param_max = 2147483646 # This is one less than INT_MAX

    if not check_if_flag_works('{0}{1}{2}{3}'.format(prefix, flag, separator, param_min)):
        print "ERROR: Flag {0} has no valid value".format(flag)
        return param_min

    value = param_max
    found = False
    while not found:
        if check_if_flag_works('{0}{1}{2}{3}'.format(prefix, flag, separator, value)):
            param_max = value
            found = True
            break
        else:
            value = abs(value - param_min)/2 + param_min
            if value > param_max or value < param_min:
                print "ERROR: Flag {0} has no valid max value".format(flag)
                value = param_min
                found = True
                break
    return value

def extract_param_defaults(input_file):
    """
    Get the default, minimum, and maximum for each compiler parameter.
    Requires source code for compiler to be in your home directory.
    This example ships with a cached version so it does not require source.
    """
    # TODO
    # default values of params need to be extracted from source code,
    # since they are not in --help. Later we'll get them from a config file,
    # as other compilers are even less helpful than GCC and are closed
    # source.
    param_defaults = dict()
    # TODO Parse the params.def file
    pattern = 'params\.def$'
    pat = re.compile(pattern)
    s = re.search(pat, input_file)
    # TODO A more intelligent check would be good
    if not s:
        log.error("This does not look like a params.def file from the GCC source")
    else:
        params_def = open(os.path.expanduser(input_file)).read()
        for m in re.finditer(r'DEFPARAM *\((([^")]|"[^"]*")*)\)', params_def):
            param_def_str = (m.group(1)
                             .replace('GGC_MIN_EXPAND_DEFAULT', '30')
                             .replace('GGC_MIN_HEAPSIZE_DEFAULT', '4096')
                             .replace('50 * 1024 * 1024', '52428800')
                             .replace('128 * 1024 * 1024', '134217728')
                             .replace('INT_MAX', '2147483646'))
            # XXX Also need to replace INT_MAX
            # With 32-bit ints, INT_MAX is 2147483647
            param_min = 0
            param_max = 0
            default = 0
            name = ""
            desc = ""
            try:
              name, desc, default, param_min, param_max = ast.literal_eval(
                  '[' + param_def_str.split(',', 1)[1] + ']')
              # TODO We need to do a search here to find the true min/max of
              # these parameters
              # In those cases where max < min, set max == min
              if param_min >= param_max:
                  param_max = find_valid_flag_max(name, '--param ', '=',
                          param_min)
              if param_max > param_min:
                  param_defaults[name] = {'default': default,
                                          'description': desc,
                                          'min': param_min,
                                          'max': param_max}
            except:
              log.exception("error with %s", param_def_str)
              log.debug("param_min is {0}\nparam_max is {1}\nname is {2}".format(param_min, param_max, name))

    return param_defaults

def preexec_setpgid_setrlimit(memory_limit):
  if resource is not None:
    def _preexec():
      os.setpgid(0, 0)
      try:
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
      except ValueError:
        pass  # No permission
      if memory_limit:
        try:
          (soft, hard) = resource.getrlimit(resource.RLIMIT_AS)
          resource.setrlimit(resource.RLIMIT_AS, (min(soft, memory_limit),
                                                  min(hard, memory_limit)))
        except ValueError:
          pass  # No permission
    return _preexec

def print_params(params, cc_param_defaults, filehandle):
    for p in params:
        filehandle.write( "        - name: {0}\n".format(p))
        filehandle.write( "          type: range\n")
        filehandle.write( "          prefix: '--param '\n")
        filehandle.write( "          separator: '='\n")
        filehandle.write( "          max: {0}\n".format(cc_param_defaults[p]['max']))
        filehandle.write( "          min: {0}\n".format(cc_param_defaults[p]['min']))

def print_flags(flags, filehandle):
    # TODO This assumes all are on/off flags, which is not true
    for f in flags:
        filehandle.write( "        - name: {0}\n".format(f[2:]))
        filehandle.write( "          type: on-off\n")
        filehandle.write( "          on-prefix: '{0}'\n".format(f[:2]))
        filehandle.write( "          off-prefix: '{0}no-'\n".format(f[:2]))

def print_heading(filehandle, compiler):
    ver = extract_compiler_version(args)
    filehandle.write("# Compiler specific settings:\n")
    filehandle.write("compiler:\n    name: {0}\n".format(compiler))
    filehandle.write("    version: {0}.{1}.{2}\n".format(ver[0], ver[1], ver[2]))
    filehandle.write("    flags:\n")

def main(args):
    if args.debug:
        log.setLevel(logging.DEBUG)

    found_compiler_flags = extract_working_flags(args)

    cc_param_defaults = extract_param_defaults(args.source)
    found_compiler_params = parse_params(args, cc_param_defaults)

    log.info("Found %d compiler flags and %d params",
            len(found_compiler_flags), len(found_compiler_params))


    # TODO dump out the flags and params and compiler version in a format that is
    # compatible with OptSearch input
    filehandle = open(args.output, 'w')
    print_heading(filehandle, args.compiler)
    print_flags(found_compiler_flags, filehandle)
    print_params(found_compiler_params, cc_param_defaults, filehandle)

    filehandle.close()
    return

if __name__ == "__main__":
    main(args)
