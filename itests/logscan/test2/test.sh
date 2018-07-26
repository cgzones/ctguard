#!/bin/sh

set -eu

cleanup () {
  rm -f test.log test.output test.pid test.state
}

cleanup

touch test.log test.state

../../../ctguard-logscan --cfg-file test.conf --unittest --foreground &
pid=$!
echo "daemon running with pid ${pid}\n"

sleep 2

#test normale logs
echo "test" >> test.log
echo "test2" >> test.log

sleep 2

#test daemon resume
pid2=`pidof ctguard-logscan` || true
if [ "X${pid}" != "X${pid2}" ]; then
  echo "daemon not running anymore!!\n${pid} vs ${pid2}\nFAILURE!!\n\n";
  exit 1
fi
kill -INT ${pid}

sleep 1

echo "test3" >> test.log

sleep 1

../../../ctguard-logscan --cfg-file test.conf --unittest --foreground &
pid=$!
echo "daemon running with pid ${pid}\n"

sleep 2


echo "test4" >> test.log

sleep 2


pid2=`pidof ctguard-logscan` || true
if [ "X${pid}" != "X${pid2}" ]; then
  echo "daemon not running anymore!!\n${pid} vs ${pid2}\nFAILURE!!\n\n";
  exit 1
fi

kill -INT ${pid}

sleep 1

diff -u test.output test.output.expected

cleanup

echo "\n\n\nsuccess!!!\n\n"
