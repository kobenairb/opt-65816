#!/bin/bash

for file in samples/*.ps; do
    echo -n "Optimizing $file "
    valgrind --quiet \
        --leak-check=full \
        --track-origins=yes \
        --exit-on-first-error=yes \
        --error-exitcode=1 \
        ./opt-65816 ${file} >/dev/null
    if [[ $? -eq 0 ]]; then
        echo "[OK]"
    else
        echo "[KO]"
        exit $?
    fi
done
