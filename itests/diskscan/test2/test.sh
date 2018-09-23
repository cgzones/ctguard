#!/bin/sh

set -eu

umask 0022

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "$SCRIPTPATH"

echo "TEST #2 START"

cleanup () {
  rm -Rf test_dir/
  rm -f test.db test.output test_diff.db
}

cleanup

mkdir test_dir/
echo "test" > test_dir/txt1.txt
echo "test" > test_dir/txt3.txt

if [ ! -x ../../../ctguard-diskscan ]; then
	echo "TEST #2 Can not find executable!"
	exit 1
fi
../../../ctguard-diskscan -c test.conf -x -f &
pid=$!
echo "TEST #2 Daemon running with pid ${pid}"

sleep 2

echo "do1" >> test_dir/txt1.txt
sleep 0.001
mkdir test_dir/testdir
sleep 0.001
touch test_dir/dest
sleep 0.001
ln -s dest test_dir/lnk

sleep 2

echo "do2" >> test_dir/txt1.txt

sleep 2

echo "do3" >> test_dir/txt1.txt
sleep 0.001
rmdir test_dir/testdir
sleep 0.001
rm test_dir/dest

sleep 2

echo "do4" >> test_dir/txt1.txt

sleep 2

kill -2 ${pid}

sleep 2

echo "TEST #2 Comapring expected vs actual output"
diff -u test.expected test.output

cleanup

echo "TEST #2 success"
