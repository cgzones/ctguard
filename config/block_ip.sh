#!/bin/sh

set -eu

if [ $# -ne 1 ]; then
	exit 2;
fi

nft add element inet filter blacklist4 { $1 timeout 360s }
