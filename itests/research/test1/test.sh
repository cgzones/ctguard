#!/bin/sh

set -eu

cleanup () {
	rm -f test.output
}

cleanup


OUTPUT=$(../../../ctguard-research --cfg-file test.conf --input -f << EOF
test
test4
4test
Sep 24 12:10:03 desktopdebian unix_chkpwd[001]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[002]: password check failed for user (root)
Sep 24 12:10:03 desktopdebian unix_chkpwd[003]: password check failed for user (chris)
Sep 24 12:10:03 desktopdebian unix_chkpwd[004]: password check failed for user (chris)
Sep 24 12:10:03 desktopdebian unix_chkpwd[005]: password check failed for user (chris)
Sep 24 12:10:03 desktopdebian unix_chkpwd[006]: password check failed for user (thomas)
Sep 24 12:10:03 desktopdebian unix_chkpwd[007]: password check failed for user (thomas)
Sep 24 12:10:03 desktopdebian unix_chkpwd[008]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[009]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[010]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[011]: password check failed for user (christian)
Sep 24 12:10:03 desktopdebian unix_chkpwd[012]: password check failed for user (chris)
Sep 24 12:10:03 desktopdebian unix_chkpwd[013]: password check failed for user (chris)
test
abc
def
xtest1
xtest2
ytestABCy
ytesty
testyy teststring1
testxx teststring2
quit
EOF
)
echo "$OUTPUT" > test.output

echo "Comapring expected vs actual output:"
diff -u test.output.expected test.output

cleanup

echo "\n\n\nsuccess!!!\n\n"
