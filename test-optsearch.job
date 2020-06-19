#!/usr/bin/env bash
#SBATCH --account=free
#SBATCH --job-name=optsearch-test
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=16
#SBATCH --time=06:00:00
#SBATCH --signal=USR1@360
#SBATCH --exclusive
#SBATCH --dependency=singleton

## Set up the environment
. /etc/profile.d/modules.sh
module purge

# This needs to be customised by machine.
module add slurm
module add gsl
module unload gcc
module add use.own
module add gcc/7.3.0
module add mvapich2/gcc-7.1.0/2.3b-noslurm

export CC=mpicc
export FC=mpif90

cd $SLURM_SUBMIT_DIR

# Make sure we're building with the right MPI and compiler
make deepclean all

# Build the test executable
cd test
# The clean should be unnecessary, but we'll do it anyway just in case
make clean test
# Copy the helloworld by hand if not using "make runtest"
cp -v ../src/helloworld.c .

#Generate a hostfile from the slurm node list
export SLURM_NODEFILE=`perl $HOME/scratch/generate_pbs_nodefile.pl`
module unload slurm

for i in $(cat $SLURM_NODEFILE | uniq)
do
    # Force use of IB
    echo "$i.ib.balena" >> hostfile
done
echo "Running `date`, on nodes: $SLURM_NODELIST. Hostfile:"
cat hostfile

echo "----------------------------------------------------------------------"

mpirun_rsh -ssh -np $SLURM_NNODES -hostfile hostfile ./build/optsearch -v -c config.yml
echo ""
echo "----------------------------------------------------------------------"
rm -f $SLURM_NODEFILE
rm -f hostfile
