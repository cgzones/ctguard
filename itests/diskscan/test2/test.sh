#!/bin/sh

set -eu

umask 0022

cleanup () {
  rm -Rf test_dir/
  rm -f test.db test.output test_diff.db
}

cleanup

mkdir test_dir/
echo "test" > test_dir/txt1.txt
echo "test" > test_dir/txt3.txt

../../../ctguard-diskscan -c test.conf -x -f &
pid=$!
echo "daemon running with pid ${pid}\n"

sleep 2

echo "do1" >> test_dir/txt1.txt
mkdir test_dir/testdir
touch test_dir/dest
ln -s dest test_dir/lnk

sleep 2

echo "do2" >> test_dir/txt1.txt

sleep 2

echo "do3" >> test_dir/txt1.txt
rmdir test_dir/testdir
rm test_dir/dest

sleep 2

echo "do4" >> test_dir/txt1.txt

sleep 2

pid2=`pidof ctguard-diskscan`
if [ "X${pid}" != "X${pid2}" ]; then
  echo "daemon not running anymore!!\n${pid} vs ${pid2}\nFAILURE!!\n\n";
  exit 1
fi

kill -2 ${pid}

sleep 2

echo "Comapring expected vs actual output:"
diff -u test.result test.output

cleanup

echo "\n\n\nsuccess!!!\n\n"
