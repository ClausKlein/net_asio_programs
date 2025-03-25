#!/bin/sh

set -x
set -u
set -e

echo '!@#$%^&*()_~<>' > base64.dat
cat base64.dat | ./base64encode - > base64.txt
cat base64.dat | ./base64encode - | ./base64decode - | diff base64.dat -
./base64encode 2>&1 | grep -w usage
./base64decode 2>&1 | grep -w usage
rm -rf tmp
./base64encode base64.txt tmp/output_file 2>&1 | grep -w output_file
./base64decode base64.dat tmp/output_file 2>&1 | grep -w output_file
./base64encode base64.dat base64-out.txt
./base64decode base64.txt base64-out.dat
diff -u base64.txt base64-out.txt
diff -u base64.dat base64-out.dat
