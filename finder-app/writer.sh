#!/bin/sh

if [ "$#" -ne 2 ]; then
	echo "Usage: $0 [WRITEFILE] [WRITESTR]"
    exit 1
else
	WRITEFILE=$1
	WRITESTR=$2
fi

# create file dir if it does not exist
DIR="$(dirname "${WRITEFILE}")"
mkdir -p ${DIR}

# create a new file with the content WRITESTR
COMMAND="echo "${WRITESTR}" > ${WRITEFILE}"
if !  eval ${COMMAND} ; then
    echo Failed to run: ${COMMAND}
    exit 1
fi
