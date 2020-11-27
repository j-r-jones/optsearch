#!/usr/bin/env bash

## Set up the environment
source ./hope/common.sh
export PROCS=2

# Delimit debugging info to make it more legible
echo "----------------------------------------------------------------------"
echo "Starting in directory `pwd`"
echo ""
echo "Compilers are:"
echo -n "CC: "
which $CC
mpicc --version
echo ""
echo -n "FC: "
which $FC
mpif90 --version
echo ""
echo "using modules:"
module list
echo "----------------------------------------------------------------------"

ldd ./tuner

echo "Running `date`, with $PROCS ranks"

echo "----------------------------------------------------------------------"
mpirun -np $PROCS optsearch -c ./hope/config.yml
echo "----------------------------------------------------------------------"

rm -rf $TMP hostfile $SLURM_NODEFILE

#[[ -f optsearch.sqlite ]] && mv -v optsearch.sqlite ./hope-results/hope-pinned-mvapich/optsearch/optsearch.sqlite.$SLURM_JOBID
