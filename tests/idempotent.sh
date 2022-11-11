#!/bin/bash

# This script is used during the migration process
# from the python script to the C version.
# It will be removed (or adapted) at the end.

function f_clean {
    rm -f tests/samples/*.log >/dev/null 2>&1
}

# MAIN

f_clean

make >/dev/null 2>&1

echo -e "\n==> Perform tests...\n"

for file in tests/samples/*.ps; do
    echo -n "$file "

    OPT816_QUIET=1 ./tests/816-opt.py "${file}" >"${file}.p.log"
    ./opt-65816 "${file}" >"${file}.c.log"

    if diff "${file}.p.log" "${file}.c.log" >/dev/null 2>&1; then
        echo "[PASS]"
        f_clean
    else
        echo "[FAILED]"
        exit 1
    fi
done
