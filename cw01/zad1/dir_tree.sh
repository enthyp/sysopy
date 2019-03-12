#!/bin/bash

###
# Grow a directory tree with files of given name
# at leave level. Use with caution.
# 
# breadth^depth leave files and 
# (1 - breadth^(depth + 1)) / (1 - breadth) 
# directories are created.
#
# Arguments:
# 	depth 
#	breadth 
#	dir_name_prefix 
#	file_name
#	root_name 
# Returns:
# 	None
# 
###

function build_tree {
	if [ $1 -eq 0 ]; then
		touch "$4"
	else
		local i=0
		while [ $i -lt $2 ]; do
			mkdir -p "$3.$i"
			cd "$3.$i"
			build_tree $[$1 - 1] $2 $3 $4
			cd ..
			i=$[$i + 1]
		done
	fi
}

if [ ! -d "$5" ]; then
	mkdir "$5"
	cd "$5"
	build_tree "$@"
	cd ..
fi
