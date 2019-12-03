#!/bin/sh

set -eu

BIN=../../../src/research/ctguard-research
if ! [ -e ${BIN} ]; then
    echo "Could not find binary at '${BIN}'!"
    exit 1
fi

cleanup () {
    rm -f test.output
}

cleanup

chmod 640 rules.xml test.conf

OUTPUT=$(${BIN} --cfg-file test.conf --input -f << EOF
Aug 23 04:59:58 server02 sshd[8268]: Bad protocol version identification '\026\003\001\0018\001' from 118.193.31.181 port 41185
quit
EOF
)
echo "$OUTPUT" > test.output

echo "Comapring expected vs actual output:"
diff -u test.output.expected test.output

cleanup

echo "SUCCESS!"
