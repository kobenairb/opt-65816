#!/bin/bash

# This script is used during the migration process
# from the python script to the C version.
# It will be removed (or adapted) at the end.

make >/dev/null 2>&1

for file in samples/*.ps; do
    echo -n "$file "
    OPT816_QUIET=0 ./816-opt.py "${file}" >"${file}.p.log"
    ./opt-65816 "${file}" >"${file}.c.log"
    diff "${file}.p.log" "${file}.c.log" >/dev/null 2>&1
    rc=$?
    if [[ $rc -eq 0 ]]; then
        echo "[OK]"
    else
        echo "[KO]"
        exit 1
    fi
done

[[ rc -eq 0 ]] || rm -f samples/*.ps.p.log samples/*.ps.c.log >/dev/null 2>&1
