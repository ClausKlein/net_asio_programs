# Standard stuff

.SUFFIXES:

MAKEFLAGS+= --no-builtin-rules
MAKEFLAGS+= --warn-undefined-variables

.PHONY: all format test check distclean

all: build
	ninja -C build

clean: build
	- ninja -C $< $@
	- find $< -name '*.gcda' -delete

distclean: # XXX clean
	rm -rf build coverage/* *~ ctags

build: CMakeLists.txt
	cmake -S . -B $@ -D CMAKE_BUILD_TYPE=Debug

check: all
	run-clang-tidy -p build *.cpp

fix: all
	run-clang-tidy -p build -fix \
	 -checks='-*,\
hicpp-explicit-conversions,\
hicpp-member-init,\
hicpp-named-parameter,\
modernize-deprecated-headers,\
modernize-loop-convert,\
modernize-use-nodiscard,\
modernize-use-std-print,\
modernize-use-trailing-return-type,\
performance-avoid-endl,\
performance-unnecessary-value-param,\
readability-avoid-const-params-in-decls,\
readability-braces-around-statements,\
readability-container-data-pointer,\
readability-else-after-return,\
readability-make-member-function-const,\
readability-redundant-member-init,\
readability-simplify-boolean-expr,\
readability-use-std-min-max,\
' \
	 *.cpp

test: all
	-killall async_tcp_echo_server
	-echo | build/async_tcp_echo_client localhost 8000
	build/async_tcp_echo_server 8000 &
	-(cat rrcp.txt | build/async_tcp_echo_client localhost 8000) &
	sleep 1
	-killall async_tcp_echo_server
	-(echo | build/async_tcp_echo_server 8000) &
	cat rrcp.txt | build/rrcp_client localhost 8000
	cat rrcp.txt | build/rrcp_async_tcp_client localhost 8000
	cat rrcp.txt | build/async_tcp_echo_client localhost 8000
	-build/async_tcp_echo_client localhost
	-echo | build/async_tcp_echo_client localhost 8001
	cat rrcp.txt | build/blocking_tcp_echo_client localhost 8000
	ctest --test-dir build
	-killall async_tcp_echo_server
	gcovr

format: .clang-format
	git ls-files ::*.cpp ::*.hpp | xargs clang-format -i
	git ls-files ::*CMakeLists.txt | xargs gersemi -i

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
