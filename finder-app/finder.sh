#!/bin/sh

if [ "$#" -ne 2 ]; then
	echo "Usage: $0 [FILESDIR] [SEARCHSTR]"
    exit 1
elif ! [ -d "$1" ]; then
	echo $1 must be a directory
	exit 1
else
	FILESDIR=$1
	SEARCHSTR=$2
fi

# total files in directory and subdirectories
TOTALFILES=$(find ${FILESDIR} -type f | wc -l)

# total matching lines in files within the directory and subdirectory
MATCHINGLINES=$(grep -r "${SEARCHSTR}" ${FILESDIR} | wc -l)

echo "The number of files are ${TOTALFILES} and the number of matching lines are ${MATCHINGLINES}"
