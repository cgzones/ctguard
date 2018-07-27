#!/bin/sh

set -eu

umask 0022

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "$SCRIPTPATH"

echo "TEST #1 START"

cleanup () {
  rm -Rf test_dir/
  rm -f test.db test.output test5.txt test_diff.db
}

cleanup

mkdir test_dir/
echo "test" > test_dir/txt1.txt
echo "test" > test_dir/txt3.txt
ln -s txt1.txt test_dir/lnk1
touch test_dir/dead
ln -s dead test_dir/lnk2
rm test_dir/dead

if [ ! -x ../../../ctguard-diskscan ]; then
	echo "TEST #1 Can not find executable!"
	exit 1
fi
../../../ctguard-diskscan -c test.conf -x -f &
pid=$!
echo "TEST #1 Daemon running with pid ${pid}"

sleep 1

echo "2" >> test_dir/txt1.txt
sleep 0.001
echo "test" > test_dir/txt2.txt
sleep 0.001
mkdir test_dir/testsub/
sleep 0.001
echo "test" > test_dir/testsub/txt.txt
sleep 0.001
rm test_dir/txt3.txt

sleep 1

cp test_dir/txt1.txt test_dir/testsub/
mv test_dir/testsub/txt1.txt test_dir/testsub/txt3.txt
sleep 0.001
mkdir -p test_dir/testsub/testsubsub/sub

sleep 1
rm -Rf test_dir/testsub

sleep 1

echo "test5" > test5.txt
sleep 0.001
mv test5.txt test_dir/
sleep 0.001
ln -s test5.txt test_dir/lnk3
sleep 0.001
rm test_dir/lnk1

sleep 1

mv test_dir/test5.txt .

sleep 1

kill -2 ${pid}

sleep 2

echo "TEST #1 Comapring expected vs actual output"
diff -u test.result test.output

cleanup

echo "TEST #1 success"
