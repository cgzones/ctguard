#!/bin/sh

set -eu

BIN=../../../src/diskscan/ctguard-diskscan
if ! [ -e ${BIN} ]; then
    echo "Could not find binary at '${BIN}'!"
    exit 1
fi

umask 0022

cleanup () {
  rm -Rf test_dir/
  rm -f test.db test.output test.output.sorted test_diff.db
}

cleanup

chmod 640 test.conf

mkdir test_dir/
echo "test" > test_dir/txt1.txt
echo "test" > test_dir/txt3.txt

${BIN} -c test.conf -x -f &
pid=$!
echo "TDaemon running with pid ${pid}."

trap "kill -9 ${pid}" 0 2

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

if ! ps -p ${pid} > /dev/null; then
    echo "diskscan daemon with pid ${pid} not running anymore!\nFAILURE!"
    exit 1
fi

kill -INT ${pid}
trap - 0 2

sleep 2

echo "Comapring expected vs actual output:"
USERNAME=$(id -un)
sed "s/ITEST_USER_REPLACEME/${USERNAME}/g" test.expected.sample > test.expected
sort test.output -o test.output.sorted
diff -u test.expected test.output.sorted

cleanup

echo "SUCCESS!"
