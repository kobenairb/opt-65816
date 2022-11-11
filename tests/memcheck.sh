#!/bin/bash

echo -e "\n==> Perform memory check...\n"

for file in tests/samples/*.ps; do
    echo -n "$file "

    if valgrind --quiet \
        --leak-check=full \
        --track-origins=yes \
        --exit-on-first-error=yes \
        --error-exitcode=1 \
        ./opt-65816 "${file}" >/dev/null; then
        echo "[PASS]"
    else
        echo "[FAILED]"
        exit 1
    fi
done
