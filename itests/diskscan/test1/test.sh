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
  rm -f test.db test.output test.output.sorted test5.txt test_diff.db
}

cleanup

chmod 640 test.conf

mkdir test_dir/
echo "test" > test_dir/txt1.txt
echo "test" > test_dir/txt3.txt
ln -s txt1.txt test_dir/lnk1
touch test_dir/dead
ln -s dead test_dir/lnk2
rm test_dir/dead

${BIN} -c test.conf -x -f &
pid=$!
echo "Daemon running with pid ${pid}."

trap "kill -9 ${pid}" 0 2

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

if ! ps -p ${pid} > /dev/null; then
    echo "diskscan daemon with pid ${pid} not running anymore!\nFAILURE!"
    exit 1
fi

kill -INT ${pid}
trap - 0 2

sleep 2

echo "Comapring expected vs actual output:"
USERNAME=$(id -un)
GROUP=$(id -gn)
sed -e "s/ITEST_USER_REPLACEME/${USERNAME}/g" test.expected.sample | sed -e "s/ITEST_GROUP_REPLACEME/${GROUP}/g" > test.expected
sort test.output -o test.output.sorted
diff -u test.expected test.output.sorted

cleanup

echo "SUCCESS!"
