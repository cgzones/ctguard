#!/bin/sh

set -eu

cleanup () {
	rm -f test.output
}

cleanup


OUTPUT=$(../../../ctguard-research --cfg-file test.conf --input -f << EOF
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (root)
Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)
quit
EOF
)
echo "$OUTPUT" > test.output

echo "Comapring expected vs actual output:"
diff -u test.output.expected test.output

cleanup

echo "\n\n\nsuccess!!!\n\n"
