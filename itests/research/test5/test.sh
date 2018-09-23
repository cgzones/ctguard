#!/bin/sh

set -eu

cleanup () {
	rm -f test.output
}

cleanup


OUTPUT=$(../../../ctguard-research --cfg-file test.conf --input -f << EOF
Aug 23 04:59:58 server02 sshd[8268]: Bad protocol version identification '\026\003\001\0018\001' from 118.193.31.181 port 41185
quit
EOF
)
echo "$OUTPUT" > test.output

echo "Comapring expected vs actual output:"
diff -u test.output.expected test.output

cleanup

echo "\n\n\nsuccess!!!\n\n"
