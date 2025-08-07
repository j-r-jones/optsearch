# OptSearch

Search for an optimal set of compiler flags for a given program and compiler.
Config files are supplied in YAML format, and should contain a number of
useful settings.  These include the parameters and flags used to control
optimisations of the compiler, which compiler is being used, and how to run
the two required tests:

* One for accuracy, to check that compiler optimisations have not ruined
  things;
* One for performance, to see whether the chosen set of flags and parameters
  have affected things in the way hoped.

The tests are assumed to be run as scripts, such as bash scripts, and are
naively run and timed using the available utilities from `time.h` and
`system()` calls.

The search itself is done by a particle swarm optimiser, based on [SPSO 2011
as described by Clerc](http://clerc.maurice.free.fr/pso/SPSO_descriptions.pdf),
but with some alterations for this particular type of search problem.

For more information, please refer to the related PhD thesis:

["Autotuning Compiler Options for HPC"](https://researchportal.bath.ac.uk/en/studentTheses/auto-tuning-compiler-options-for-hpc)

Or search for the DOI: 10.13140/RG.2.2.35053.33761

## Assumptions
As over 95% of the world's publicly known supercomputers run the Linux
operating system, this assumed to be the target platform.  Others have not
been tested but may work.

## History and Goals
This project was originally conceived in about 2007, after it became clear
that many people were not able to drive the available compilers as well as
might be hoped.  In the HPC environment, this is much more of a problem than
it is on the average desktop, where packages tend to be compiled for generic
targets in any case.

Bleeding edge hardware often has no high performance libraries for at least 6
months after it becomes available.  Supercomputing centres like to try to stay
ahead of the curve, and will buy this hardware as soon as they can.  This
presents a problem where everything runs sub-optimally for many, many months.
A terrible waste of core hours, power, etc.

In the day to day life of a supercomputer sysadmin,
software compilation is a common task, and one gets to know the compiler
pretty thoroughly if attempting to do the job well.  Many significant
improvements can be obtained simply by knowing the target code well, and
carefully choosing compiler flags and parameters from reading the manual and
the source code (of the compiler).  This is a difficult skill to transfer, and
is largely learnt "on the job".  Hence it was decided that a tool ought to be
provided.

Through this tool it should be possible for most competent builders
of software to compile programs more optimally for a specific machine, and a
stop-gap solution so be provided via tuning of reference library
implementations.

Such tools are not entirely new.  There is a little prior art.
[ATLAS](https://github.com/math-atlas/math-atlas) contains
a fairly simple one, which searches a very small set of compiler flags.
During the writing of this project, several other similar projects have
appeared.  [OpenTuner](https://github.com/jansel/opentuner) currently also includes a
simple example for tuning GCC parameters and flags.  Other projects go into
their own domains in more detail, often targeting specific hardware such
as GPUs.

OptSearch is a total rewrite of what was previously a proof of concept (PoC).  First
written rather hurriedly and messily in Python, and later rewritten to use the facilities
provided by OpenTuner, it became clear that a different approach was needed.  There
were many problems with those PoCs, not least that they were neither very portable, nor
particularly efficient.  Not ideal in something intended to run on emerging architectures.

Python, and particularly the various libraries required by a large framework
like OpenTuner, often takes a while to get ported.  As a result, you cannot
provide better performing libraries soon after buying bleeding edge
hardware.

## Current dependencies:
* [libyaml](https://github.com/yaml/libyaml)
* [MPI](https://www.mpi-forum.org/) ([MVAPICH](http://mvapich.cse.ohio-state.edu/) preferred; OpenMPI currently does
  not work correctly for OptSearch, but will be fine if used for the target program)
* [GNU Scientific Library](https://www.gnu.org/software/gsl/)
* [SQLite](https://sqlite.org/index.html) (included in the source tree; used for checkpointing)

## Building
You will need GNU Make to build it, and of course a compiler.  GCC is what has
been used during development.  It should build correctly, assuming the right
library paths, etc are set in make.inc (see the included example), if you
simply:
```
  $ make
```
You may need to set `$LD_LIBRARY_PATH` to run it.  The executable will be
found in the build directory.  You may want to copy the YAML files from the
examples directory as a starting point.
```
./optsearch -c config.yml
```
At present it is assumed that the flags and params YAML files will be included
or found within the main config file.  Please look at the examples if you are
unsure.

Note that, although checkpoints are taken regularly, optsearch also listens
for a user-customisable signal (default SIGUSR1) to trigger a checkpoint.
This allows for the use of ```#SBATCH --signal``` so that a checkpoint is
immediately triggered before the walltime is reached.

Data is stored in SQLite, tuned to be as resilient as possible.

## Wishlist
* autoconf/automake
* Testing of performance via hardware counters (eg using perf)

## Health warning
This was written late at night under the influence of strong coffee by someone
suffering from severe sleep deprivation.  Expect many bugs.  No guarantees are
made or responsibility taken for your use of this software.

## Known bugs
* It is difficult to check if we have enough memory to read in a given config,
  so you could run out.  This is to be fixed.
* If you include a YAML file inside itself or otherwise have circular
  includes, these cannot currently be detected and you will have a program
  that uses up all memory and fails to terminate. Hopefully it will end up
  being OOM-killed before anything else breaks, but this is not guaranteed and
  is not something over which I have any control at present.
  This will be detected (and handled appropriately) in a later version, but
  time pressures mean it has to be tolerated for now.  It is up to you to
  avoid doing anything stupid.
* Logging needs some serious work .. possibly replacement.  It is broken,
  so best not to turn it up beyond 'debug' (and even that is risky).
* If the SQLite file exists but is empty, OptSearch will spin doing nothing
  useful until you kill it or send it the quit signal.
