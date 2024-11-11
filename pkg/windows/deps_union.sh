#!/bin/bash

dependencies=()

for obj in "$@"; do
    maybe_dep="$(dirname "$obj")/.$(basename "$obj").dependencies"
    if [ -f "$maybe_dep" ]; then
        obj="$maybe_dep"
    fi

    dependencies+=($obj)
done

sort -u "${dependencies[@]}"



