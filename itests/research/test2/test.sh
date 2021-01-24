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
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (root)
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)
quit
EOF
)
echo "$OUTPUT" > test.output

echo "Comparing expected vs actual output:"
diff -u test.output.expected test.output

cleanup

echo "SUCCESS!"
