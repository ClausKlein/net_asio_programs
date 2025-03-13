# Standard stuff

.SUFFIXES:

MAKEFLAGS+= --no-builtin-rules
MAKEFLAGS+= --warn-undefined-variables

.PHONY: all format test check distclean

all: build
	ninja -C build

distclean:
	rm -rf build

build: CMakeLists.txt
	cmake -S . -B $@

check: all
	run-clang-tidy -p build *.cpp

fix: all
	run-clang-tidy -p build -fix \
	 -check='-*,readability-use-std-min-max,-misc-include-cleaner,cppcoreguidelines-init-variables,hicpp-member-init,-modernize-avoid-bind,readability-braces-around-statements,hicpp-named-parameter,readability-else-after-return,modernize-use-trailing-return-type,readability-redundant-member-init,misc-const-correctness' \
	 *.cpp

test: all
	-killall blocking_tcp_echo_server
	build/blocking_tcp_echo_server 8000 &
	cat rrcp.txt | build/rrcp_client localhost 8000
	cat rrcp.txt | build/async_tcp_echo_client localhost 8000

format: .clang-format
	git ls-files ::*.cpp ::*.hpp | xargs clang-format -i
	gersemi -i CMakeLists.txt

# These rules keep make from trying to use the match-anything rule below
# to rebuild the makefiles--ouch!

CMakeLists.txt :: ;
GNUmakefile :: ;
.clang-tidy :: ;
.clang-format :: ;

# Anything we don't know how to build will use this rule.  The command is
# a do-nothing command.
% :: build
	ninja -C $< $@
