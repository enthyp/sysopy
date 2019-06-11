#!/bin/bash
cat "$1" | tr ' ' '\n' | sort | uniq -c | awk '{ print $1" "$2 }' > verytmp;
mv verytmp "$1"
