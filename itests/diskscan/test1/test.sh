#!/bin/sh

set -eu

umask 0022

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

../../../ctguard-diskscan -c test.conf -x -f &
pid=$!
echo "daemon running with pid ${pid}\n"

sleep 1

echo "2" >> test_dir/txt1.txt
echo "test" > test_dir/txt2.txt
mkdir test_dir/testsub/
echo "test" > test_dir/testsub/txt.txt
rm test_dir/txt3.txt

sleep 1

cp test_dir/txt1.txt test_dir/testsub/
mv test_dir/testsub/txt1.txt test_dir/testsub/txt3.txt

mkdir -p test_dir/testsub/testsubsub/sub

sleep 1
rm -Rf test_dir/testsub

sleep 1

echo "test5" > test5.txt
mv test5.txt test_dir/
ln -s test5.txt test_dir/lnk3
rm test_dir/lnk1

sleep 1

mv test_dir/test5.txt .

sleep 1

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
