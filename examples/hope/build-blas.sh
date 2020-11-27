#! /usr/bin/env bash

source ./hope/common.sh

echo "Copying the BLAS library sources to $TMP to avoid clobbering"
mkdir -p $TMP
cp -r BLAS $TMP/

# For this to work, you need a Makefile to compile a shared version of the
# library.  I modified the one that ships with the reference BLAS.
echo "Building the BLAS library"
cd $TMP/BLAS && make libblas.so
ls -l libblas*

