#!/bin/bash

echo -e "\n==> Perform memory check...\n"

for file in tests/samples/*.ps; do
    echo -n "$file "

    export OPT816_QUIET=1

    if valgrind --quiet \
        --leak-check=full \
        --track-origins=yes \
        --exit-on-first-error=yes \
        --error-exitcode=1 \
        ./816-tcc-opt "${file}" >/dev/null; then
        echo "[PASS]"
    else
        echo "[FAIL]"
        exit 1
    fi
done
