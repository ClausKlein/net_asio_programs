# Standard stuff

.SUFFIXES:

MAKEFLAGS+= --no-builtin-rules
MAKEFLAGS+= --warn-undefined-variables

.PHONY: all format test lint

all: build
	ninja -C build

build: CMakeLists.txt
	cmake -S . -B $@

lint: build
	run-clang-tidy -p build -check='-*,readability-use-std-min-max,misc-include-cleaner' -fix rrcp_*.cpp

test: all
	-killall blocking_tcp_echo_server
	build/blocking_tcp_echo_server 8000 &
	cat rrcp.txt | build/rrcp_client localhost 8000

format: .clang-format
	git ls-files ::*.cpp ::*.hpp | xargs clang-format -i
	gersemi -i CMakeLists.txt
