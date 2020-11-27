This is a simple set of example scripts used to test OptSearch on a single
2-core desktop computer.

The BLAS library Makefile needs to be altered so that a shared library is
built.  HPL and LAPACK should be built to link dynamically against this shared
BLAS libary.

HPL is used as the benchmark test.

LAPACK provides a basic sanity test, in an attempt to prevent detrimental
changes to the floating point operations being performed by the BLAS library.

Most paths are set in the common.sh file.

The OptSearch binary should be on the PATH when the run-tuner.sh script is
called.  On a supercomputer, this script would be replaced with the job
submission script instead.

The example config file is for GCC 6.3.0.  The ''compiler'' section was
created by the Python helper script, using the output of
''gcc --help=optimize'' and the content of its params.def file (from the GCC
source).  Unfortunately, params.def does not contain all the max/min values,
nor does it always get those it contains right.  A very simple binary search
is performed by the script to find the correct limits.

This reduces the amount of configuration that must be done by hand.

Note that it is actually gfortran that is called, as the reference BLAS is
written in Fortran.  OptSearch is, as far as possible, language agnostic.

