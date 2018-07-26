#!/bin/sh

set -eu

cleanup () {
  rm -f test.log test_timeout.log test.output test.pid test.state
}

cleanup

touch test.log test_timeout.log test.state

../../../ctguard-logscan --cfg-file test.conf --unittest -f &
pid=$!
echo "daemon running with pid ${pid}\n"

sleep 1

#test normale logs
echo "test" >> test.log
echo "test2" >> test.log
echo "test" >> test_timeout.log

sleep 2

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

pid2=`pidof ctguard-logscan`
if [ "X${pid}" != "X${pid2}" ]; then
  echo "daemon not running anymore!!\n${pid} vs ${pid2}\nFAILURE!!\n\n";
  exit 1
fi

kill -INT ${pid}

sleep 2

echo "Comapring expected vs actual output:"
diff -u test.output test.output.expected

cleanup

echo "\n\n\nsuccess!!!\n\n"
