#! /usr/bin/env bash
# This uses the tests supplied with the lapack library to check that the BLAS
# library has not been rendered unusable or altered in a detrimental way by
# the compilation options chosen.

source ./hope/common.sh

echo "Copying the LAPACK library sources to $TMP to avoid clobbering"
rm -rf $TMP/LAPACK
cp -r lapack-3.5.0 $TMP/LAPACK

export LDFLAGS="-L$TMP/BLAS"

cd $TMP/LAPACK
make cleanblas_testing
make blas_testing

# Do we want to send this to /dev/null to avoid noise?  I'm not sure I want to
# lose the output as it will make later debugging tricky.
if [[ -e $TMP/LAPACK/BLAS/dblat3.out ]]
then
    if grep -i "fail" $TMP/LAPACK/BLAS/*.out
    then
        # This is to give us a sensible exit code to test for when this script is
        # called by the compiler tuner.
        echo "FAIL"
        exit 2
    else
        echo "Looks like we PASSED"
        exit 0
    fi
fi
echo "No output produced, tests must have gone wrong somewhere.  Check the logs."
exit 1

# There is a cleanblas_testing target too, but as we're using dynamic linking
# we shouldn't need to worry about that.
