#!/usr/bin/env bash

#. /etc/profile.d/modules.sh
#module purge
#module add use.own

# Not running on cluster
echo "Running standalone"
#export TOPDIR="$HOME/phd/software/baselines"
export TOPDIR="$HOME/phd/baselines"
export JOBNAME=debug-run
export JOBID=0
export PROCS=4
export NODES=1

export BLASDIR="$TOPDIR/BLAS"
export LAPACKDIR="$TOPDIR/lapack-3.5.0"

# On hope, /dev/shm is mounted noexec
#export TMP=/dev/shm/$JOBNAME-$JOBID
export TMP=/tmp/$JOBNAME-$JOBID

export TMPDIR=$TMP
export TMP_DIR=$TMP

#  We are using dynamic linking so that only the BLAS library need be
#  recompiled each time.  This saves considerable time.
export LD_LIBRARY_PATH=$TMP/BLAS:$LD_LIBRARY_PATH

export CC=gcc
export FC=gfortran
export FFLAGS="$FLAGS"
export CFLAGS="$FLAGS"

