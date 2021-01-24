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
cat > test_dir/txt1.txt <<EOF
test file
this test file has many lines
1
2
3
4
5
and it contains many entries
1
2
3
4
5
6
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
the end
EOF
cat > test_dir/txt12.txt <<EOF
est file
this test file has many lines
1
2
3
4
5
and it contains many entries
1
2
3
4
5
6
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
the end
EOF
echo "test" > test_dir/txt3.txt
ln -s txt1.txt test_dir/lnk1
touch test_dir/dead
ln -s dead test_dir/lnk2
rm test_dir/dead
touch test_dir/test_rm

${BIN} -c test.conf -x -f -S

cat > test_dir/txt1.txt <<EOF
test file
this test file has many lines
9
8
7
6
5
4
3
2
1
and it contains many entries
1
2
3
4
5
6
7
8
9
text1texttexttexttexttexttexttexttexttexttexttexttexttexttext
texttext1texttexttexttexttexttexttexttexttexttexttexttexttext
texttexttext1texttexttexttexttexttexttexttexttexttexttexttext
texttexttexttext1texttexttexttexttexttexttexttexttexttexttext
texttexttexttexttext1texttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttext1texttexttexttexttexttexttexttexttext
texttexttexttexttexttexttext1texttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
the end
really
EOF
cat > test_dir/txt12.txt << EOF
test file
this test file has many lines
1
2
3
4
5
6
7
8
9
and it contains many entries
9
8
7
6
5
4
3
2
1
text1texttexttexttexttexttexttexttexttexttexttexttexttexttext
texttext1texttexttexttexttexttexttexttexttexttexttexttexttext
texttexttext1texttexttexttexttexttexttexttexttexttexttexttext
texttexttexttext1texttexttexttexttexttexttexttexttexttexttext
texttexttexttexttext1texttexttexttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
texttexttexttexttexttext1texttexttexttexttexttexttexttexttext
texttexttexttexttexttexttext1texttexttexttexttexttexttexttext
texttexttexttexttexttexttexttexttexttexttexttexttexttexttext
the end
really
EOF
echo "test" > test_dir/txt2.txt
mkdir test_dir/testsub/
echo "test" > test_dir/testsub/txt.txt
rm test_dir/txt3.txt

cp test_dir/txt1.txt test_dir/testsub/
mv test_dir/testsub/txt1.txt test_dir/testsub/txt3.txt

mkdir test_dir/testsub/testsubsub

echo "test5" > test5.txt
mv test5.txt test_dir/
ln -s test5.txt test_dir/lnk3
rm test_dir/lnk1
rm test_dir/test_rm

${BIN} -c test.conf -x -f -S

echo "Comparing expected vs actual output:"
USERNAME=$(id -un)
GROUP=$(id -gn)
sed -e "s/ITEST_USER_REPLACEME/${USERNAME}/g" test.expected.sample | sed -e "s/ITEST_GROUP_REPLACEME/${GROUP}/g" > test.expected
LC_ALL=C sort test.output -o test.output.sorted
diff -u test.expected test.output.sorted

cleanup

echo "SUCCESS!"
