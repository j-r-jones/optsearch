#!/bin/bash

echo "Running MPI hello world test"

. /etc/profile.d/modules.sh
module purge
module add slurm
#module add use.own
#module add openmpi/gcc-7.1.0/3.0.0
module add gcc/7.1.0
module add mvapich2/gcc/2.1-qib

module list

rm mpi-helloworld
mpicc -o mpi-helloworld mpi-helloworld.c

#echo "====================="
#
#env | grep -i MPI
#
#echo "====================="

export PROCS=10

HOSTS=$(for i in `seq 2 $PROCS`; do echo -n ",$HOSTNAME"; done)
ldd ./mpi-helloworld
mpirun -np $PROCS --host $HOSTNAME$HOSTS ./mpi-helloworld
retval=$?

echo ""
echo "====================="
echo ""

source ../../baselines/common.sh

cat ../../baselines/HPL.data.one >> $TMP/HPL.dat
cp ../../baselines/hpl-2.1/bin/tuner/xhpl $TMP/
cd $TMP
export LD_LIBRARY_PATH=$TMP/BLAS:$LD_LIBRARY_PATH
ldd ./xhpl
mpirun -np $PROCS --host $HOSTNAME$HOSTS ./xhpl

echo "====================="
echo ""
exit $retval
