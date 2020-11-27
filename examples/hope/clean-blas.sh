#! /usr/bin/env bash

source ./hope/common.sh

echo "Running clean-blas.sh on `hostname` in $PWD"

echo "Removing the local copy of the BLAS library at $TMP/BLAS"
rm -rf $TMP/BLAS

exit 0
