# Usage examples

## Build with cmake

cmake -S . -G build -G Ninja
cd build
ninja

## Tests

    echo '!@#$%^&*()_~<>' > base64.dat
    cat base64.dat | ./base64encode - > base64.txt
    cat base64.dat | ./base64encode - | ./base64decode - | diff base64.dat -

hexdump -C base64.dat

    00000000  21 40 23 24 25 5e 26 2a  28 29 5f 7e 3c 3e 0a     |!@#$%^&*()_~<>.|
    0000000f

cat base64.txt

    IUAjJCVeJiooKV9+PD4K

