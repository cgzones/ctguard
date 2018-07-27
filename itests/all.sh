#!/bin/sh

set -eu

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "$SCRIPTPATH"


echo "########################################"
echo 
echo "          Starting All Tests"
echo
echo "########################################"


for dirname in */; do
	cd $dirname
	for testnr in */; do
		cd $testnr
		echo "TESTDRIVER Running test ${dirname}${testnr}"
		./test.sh
		cd ../
	done

	cd ../
done


echo "########################################"
echo
echo "          Finished All Tests"
echo
echo "########################################"
