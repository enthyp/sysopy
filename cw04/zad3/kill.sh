#!/bin/bash

to_kill=`ps aux | grep "sender\|catcher" | grep -v "grep" | awk '{print $2}'`

if [ ! -z "$to_kill" ]; then
	kill -9 "$to_kill";
fi
