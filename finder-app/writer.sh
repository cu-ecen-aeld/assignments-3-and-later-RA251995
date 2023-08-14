#!/bin/sh

if [ $# -ne 2 ]
then
	echo "Usage: $0 <writefile> <writestr>"
	exit 1
fi

writefile=$1
writestr=$2

writefilepath=$(dirname $writefile)
mkdir -p $writefilepath
echo $writestr > $writefile
