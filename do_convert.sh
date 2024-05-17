#!/bin/sh

if [ x$1 == "x" ] ; then
	echo Usage: $0 run_number
	exit 10
fi

./makelist_prop $1
./makelist_vme  $1
./makelist_sync $1
./pconvert      $1
./dat2rootp     Strip.conf $1
