#!/bin/bash

###
# Grow a directory tree with files of given name
# at leave level. Use with caution.
#
# Arguments:
# 	depth 
#	breadth 
#	dir_name_prefix 
#	file_name
# Returns:
# 	None
# 
###

function build_tree {
	if [ $1 -eq 0 ]
	then
		touch "$4"
	else
		local i=0
		while [ $i -lt $2 ] 
		do
			mkdir "$3.$i"
			cd "$3.$i"
			build_tree $[$1 - 1] $2 $3 $4
			cd ..
			i=$[$i + 1]
		done
	fi
}

mkdir file_tree
cd file_tree

build_tree "$@"

cd ..

