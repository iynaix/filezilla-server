#!/bin/sh

trap 'rm -f "$res" "$res".tmp' 0 2 3 15
res=$(mktemp)

obj="$1"

if [ -n "$obj" ] ; then
    maybe_dep="$(dirname "$obj")/.$(basename "$obj").dependencies"
    if [ -f "$maybe_dep" ]; then
        obj="$maybe_dep"
    fi
    
    cat "$obj" > "$res"
    shift
fi

for obj in "$@"; do
    maybe_dep="$(dirname "$obj")/.$(basename "$obj").dependencies"
    if [ -f "$maybe_dep" ]; then
        obj="$maybe_dep"
    fi

    comm -12 "$res" "$obj" > "$res".tmp; mv "$res".tmp "$res"; 
done; 

cat "$res"



