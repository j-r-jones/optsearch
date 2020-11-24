

Step 1: Generate the compiler flag part of the input file for OptSearch using the helper Python 2.7 script.

To do this, you will need params.def from the relevant GCC version that you are using.  This can be downloaded from one of the GCC mirror sites.  Because I am tuning the reference BLAS in this example, it is gfortran that is of interest (the reference BLAS is written in Fortran).

  $ gfortran --version
  GNU Fortran (GCC) 8.1.0 20180502 (Cray Inc.)
  Copyright (C) 2018 Free Software Foundation, Inc.
  This is free software; see the source for copying conditions.  There is NO
  warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  $ python2.7 ./optsearch/scripts/gcc_to_config.py -c gfortran -t ./optsearch/test/helloworld.f -s ./gcc-8.1.0/gcc/params.def -o gcc-8.1.0-config.yaml
  2020-11-24 16:32:40,773 removing flag -fhandle-exceptions because renamed
  2020-11-24 16:33:16,734 removing flag -fno-stack-protector-all because it results in compile error
  2020-11-24 16:33:16,735 Odd... -fstack-protector-all works but -fno-stack-protector-all does not
  2020-11-24 16:33:16,970 removing flag -fno-stack-protector-explicit because it results in compile error
  2020-11-24 16:33:16,970 Odd... -fstack-protector-explicit works but -fno-stack-protector-explicit does not
  2020-11-24 16:33:17,205 removing flag -fno-stack-protector-strong because it results in compile error
  2020-11-24 16:33:17,205 Odd... -fstack-protector-strong works but -fno-stack-protector-strong does not
  2020-11-24 16:33:37,458 removing flag -mbig-endian because it results in compile error
  2020-11-24 16:34:09,174 removing flag --param integer-share-limit=2147483646 because it results in compile error
  2020-11-24 16:34:10,293 removing flag --param integer-share-limit=1073741824 because it results in compile error
  2020-11-24 16:34:12,815 removing flag --param integer-share-limit=536870913 because it results in compile error
  2020-11-24 16:34:16,840 removing flag --param integer-share-limit=268435457 because it results in compile error
  2020-11-24 16:34:22,117 removing flag --param integer-share-limit=134217729 because it results in compile error
  2020-11-24 16:34:39,941 removing flag --param min-nondebug-insn-uid=2147483646 because it results in compile error
  2020-11-24 16:34:41,211 removing flag --param min-nondebug-insn-uid=1073741823 because it results in compile error
  2020-11-24 16:34:46,310 removing flag --param min-nondebug-insn-uid=536870911 because it results in compile error
  2020-11-24 16:35:28,158 removing flag --param=sched-autopref-queue-depth=-1 because it results in compile error
  2020-11-24 16:35:33,894 removing flag --param=vect-max-peeling-for-alignment=-1 because it results in compile error
  $

Step 2. Add the other required sections of the config file.  You can use the basic examples for this.  The missing parts need to go at the top of the one that was just generated.  For example:

 ---
 ## Main config file for optsearch
 # These are simple scripts intended to make configuration slightly easier.
 quit-signal: SIGUSR1 # see signal(7) manpage for the list of signals. By default slurm will send SIGTERM 30 seconds before SIGKILL
 clean-script: ./clean-blas.sh # Clean the build directory, etc, before beginning each run
 build-script: ./build-blas.sh # equiv to make; must use environment variable FLAGS to be set and to then affect the compilation via CFLAGS or FFLAGS, etc depending on which language is being compiled.
 accuracy-test: ./test-blas.sh # Run some tests to check that numerical results are not adversely affected by compiler optimisations.  This should return an integer. 0 is success.
 performance-test: ./run-hpl.sh # Run a benchmark.  Return a measurement, such as run time, that can be used to assess fitness against other tests.  Lower is better, but 0 is probably a failure or error.
 epsilon: 5.0 # Allowed/expected experimental error (this is not a percentage, so you need to know what this is)
 timeout: 360 # How long to wait for commands to run before killing the spawned compilation process, in seconds
 benchmark-timeout: 3600  # How long to wait before killing the spawned benchmark process, in seconds
 benchmark-repeats: 20  # Maximum number of times to repeat the benchmark if timing results do not converge
 # The rest of this file should have been generated using the script in step 1

Step 3. Make sure that your paths to any helper scripts in the config file are correct, and that the scripts work.  Pay attention also to any timeouts -- you will need to know how long your target application takes to run.  In the example HPL, a test run with BLAS compiled with no optimisation was used to get a realistic worst-case time.  You could set it shorter than this.

Compilation time is a bit more difficult to guess.

Step 4. Run OptSearch using the system's job scheduler.  OptSearch uses MPI and can expand to make use of thousands of nodes at a time.  It has been tested up to 1024 nodes so far, because of the limit on what was available.  As the search space is so vast, it should be possible to fill any machine currently on the top500 list.

This example runs on just 4 nodes.  A test run might run on only 2 (the minimum required).

 $ mpirun -n 4 ./optsearch/build/optsearch -v -c gcc-8.1.0-config.yml


