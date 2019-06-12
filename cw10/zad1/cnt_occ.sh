#!/bin/bash
cat "$1" | tr ' ' '\n' | sort | uniq -c | awk '{print $1" "$2}' > file1;
(echo -n "Total: "; cat file1 | awk '{s+=$1} END {print s}') > "$1";
cat file1 >> "$1";
rm file1
