#! /usr/bin/env bash

## Set up the environment
source ./hope/common.sh

# Delimit debugging info to make it more legible
echo "----------------------------------------------------------------------"
echo "Starting in directory `pwd`"
echo ""
echo "Flags used are: CFLAGS=$CFLAGS ; FFLAGS=$FFLAGS ;"
echo "BLASDIR is $BLASDIR"
echo ""
echo "Compilers are:"
echo -n "CC: "
which $CC
$CC --version
echo ""
echo -n "FC: "
which $FC
$FC --version
echo ""
echo "using modules:"
module list
echo ""
echo "----------------------------------------------------------------------"

mkdir -vp $TMP

FLAGS=$FLAGS ./hope/clean-blas.sh
FLAGS=$FLAGS ./hope/build-blas.sh
FLAGS=$FLAGS ./hope/test-blas.sh

echo "----------------------------------------------------------------------"

echo -n "Running xhpl on `date`, on $PROCS processors on nodes: $NODES"

echo "----------------------------------------------------------------------"

./hope/run-hpl.sh

echo "----------------------------------------------------------------------"
