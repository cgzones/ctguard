#!/bin/sh

set -eu

BIN=../../../src/logscan/ctguard-logscan
if ! [ -e ${BIN} ]; then
    echo "Could not find binary at '${BIN}'!"
    exit 1
fi

cleanup () {
    rm -f test.log test_timeout.log test.output test.pid test.state
}

cleanup

chmod 640 test.conf
touch test.log test_timeout.log test.state

${BIN} --cfg-file test.conf --unittest -f &
pid=$!
echo "Daemon running with pid ${pid}."

trap "kill -9 ${pid}" 0 2

sleep 0.5

#test normale logs
echo "test" >> test.log
echo "test2" >> test.log
echo "test" >> test_timeout.log

sleep 1

#test truncation1
echo "" > test.log
echo "test3" >> test.log

sleep 2

#test truncation2
echo "" > test.log
sleep 2
echo "test4" >> test.log

sleep 2

#test remove
rm test.log
sleep 2
echo "test5" >> test.log


sleep 2

if ! ps -p ${pid} > /dev/null; then
    echo "Daemon with pid ${pid} not running anymore!\nFAILURE!";
    exit 1
fi

kill -INT ${pid}
trap - 0 2

sleep 2

echo "Comapring expected vs actual output:"
diff -u test.output test.output.expected

cleanup

echo "SUCCESS!"
