#!/bin/bash
cat "$1" | tr ' ' '\n' | sort | uniq -c | awk '{print $1" "$2}' > file"$1";
(echo -n "Total: "; cat file"$1" | awk '{s+=$1} END {print s}') > "$1";
cat file"$1" >> "$1";
rm file"$1"
