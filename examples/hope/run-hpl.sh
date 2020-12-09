#! /usr/bin/env bash

source ./hope/common.sh

# Workaround as this is not being set correctly
export PROCS=2

## This is not rebuilt so we don't really need to copy it anywhere, but we do
# it anyway just in case.
mkdir -vp $TMP
cp -v hpl-2.1/bin/tuner/xhpl $TMP/
cat hope/HPL.dat > $TMP/HPL.dat

cd $TMP
ldd ./xhpl

echo "Running xhpl on $PROCS processors on $HOSTNAME"
mpirun -np $PROCS ./xhpl

rm -f $TMP/xhpl $TMP/HPL.dat
