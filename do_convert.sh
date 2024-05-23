#!/bin/bash

if [ x$1 == "x" ] ; then
	echo Usage: $0 run_number [SiPM.conf]
	exit 10
fi

_CNF="Strip.conf"
if [ x$2 != "x" ] ; then
	_CNF=$2
fi

./makelist_prop $1
./makelist_vme  $1
./makelist_sync $1
./pconvert      $1
./dat2rootp     ${_CNF} $1
