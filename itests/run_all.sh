#!/bin/sh

set -eu

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "$SCRIPTPATH"

# satisfy file permissions checks
find . -name *.conf -exec chmod o-r {} \;

find . -name *.expected -exec sed -i -e "s/ITEST_USER_REPLACEME/$(whoami)/g" {} \;


echo "########################################"
echo 
echo "          Starting All Tests"
echo
echo "########################################"

for dirname in */; do
	cd $dirname
	for testnr in */; do
		cd $testnr
		./test.sh
		cd ..
	done
done


echo "########################################"
echo
echo "          Finished All Tests"
echo
echo "########################################"
