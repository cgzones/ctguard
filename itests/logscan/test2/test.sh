#!/bin/sh

set -eu

BIN=../../../src/logscan/ctguard-logscan
if ! [ -e ${BIN} ]; then
    echo "Could not find binary at '${BIN}'!"
    exit 1
fi

cleanup () {
    rm -f test.log test.output test.pid test.state
}

cleanup

chmod 640 test.conf
touch test.log test.state

${BIN} --cfg-file test.conf --unittest --foreground &
pid=$!
echo "Daemon running with pid ${pid}."

trap "kill -9 ${pid}" 0 2

sleep 2

#test normal logs
echo "test" >> test.log
echo "test2" >> test.log

sleep 2

#test daemon resume
if ! ps -p ${pid} > /dev/null; then
    echo "Daemon with pid ${pid} not running anymore!\nFAILURE!\n";
    exit 1
fi

kill -INT ${pid}
trap - 0 2

sleep 1

echo "test3" >> test.log

sleep 1

${BIN} --cfg-file test.conf --unittest --foreground &
pid=$!
echo "Daemon running with pid ${pid}."

trap "kill -9 ${pid}" 0 2

sleep 2


echo "test4" >> test.log

sleep 2


if ! ps -p ${pid} > /dev/null; then
    echo "Daemon with pid ${pid} not running anymore!\nFAILURE!";
    exit 1
fi

kill -INT ${pid}
trap - 0 2

sleep 1

diff -u test.output test.output.expected

cleanup

echo "SUCCESS!"
