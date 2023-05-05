#!/bin/bash

# This script is used during the migration process
# from the python script to the C version.
# It will be removed (or adapted) at the end.

function f_clean {
    rm -f tests/samples/tmp/*.asm >/dev/null 2>&1
}

# MAIN

f_clean

make >/dev/null 2>&1

echo -e "\n==> Perform optimizations ...\n"

for file in tests/samples/*.ps; do

    filename=$(basename "${file}")
    echo "Optimizing $filename"
    echo "./816-opt ${file} >tests/samples/tmp/${filename}.asm"
    ./816-tcc-opt "${file}" >"tests/samples/tmp/${filename}.asm"
    echo

done
