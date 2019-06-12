#!/bin/bash
cat "$1" | tr ' ' '\n' | sort | uniq -c | awk '{ print $1" "$2 }' > file1;
(echo -n "Total: "; cat file1 | awk '{ print $1 }' | paste -sd+ | bc ) > file2;
cat file1 >> file2;
mv file2 "$1";
rm file1
