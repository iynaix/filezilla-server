#!/bin/bash

lhs="$1"
rhs="$2"

maybe_dep="$(dirname "$lhs")/.$(basename "$lhs").dependencies"
if [ -f "$maybe_dep" ]; then
    lhs="$maybe_dep"
fi

maybe_dep="$(dirname "$rhs")/.$(basename "$rhs").dependencies"
if [ -f "$maybe_dep" ]; then
    rhs="$maybe_dep"
fi

comm -23 "$lhs" "$rhs"



