# Standard stuff

.SUFFIXES:

MAKEFLAGS+= --no-builtin-rules
MAKEFLAGS+= --warn-undefined-variables

export hostSystemName=$(shell uname)
export GCOV=llvm-cov gcov
export CPM_USE_LOCAL_PACKAGES=YES

ifeq (${hostSystemName},Darwin)
  export LLVM_PREFIX:=$(shell brew --prefix llvm)
  export LLVM_DIR?=$(shell realpath ${LLVM_PREFIX})
  export PATH:=${LLVM_DIR}/bin:${PATH}
  export CXX:=clang++

  # to test g++-15:
  #XXX export CXX:=g++-15
  #XXX export CXXFLAGS:=-stdlib=libstdc++
else ifeq (${hostSystemName},Linux)
  export LLVM_DIR?=/usr/lib/llvm-19
  export PATH:=${LLVM_DIR}/bin:${PATH}
  export CXX:=clang++-19
endif

CPPFILES:= $(shell git ls-files ::*.cpp | grep -vw tests)

PRESET_NAME?=debug
BUILD_DIR:=build/$(PRESET_NAME)

.PHONY: all format test check distclean

all: $(BUILD_DIR)
	cmake --workflow --preset $(PRESET_NAME)

clean: $(BUILD_DIR)
	-ninja -C $< $@
	-find $< -name '*.gcda' -delete

distclean: # XXX clean
	rm -rf $(BUILD_DIR) build coverage/* *~ ctags

$(BUILD_DIR): CMakeLists.txt
	-test -f CMakeUserPresets.json || ln -f -s cmake/CMakeUserPresets.json .
	cmake --preset $(PRESET_NAME) --log-level=VERBOSE  # --fresh
	# -test -d build/Debug && ln -f -s $(CURDIR)/build/Debug $(CURDIR)/$(BUILD_DIR)

check: all
	run-clang-tidy -p $(BUILD_DIR) $(CPPFILES)

fix: all
	run-clang-tidy -p $(BUILD_DIR) -fix -checks='-*,\
hicpp-explicit-conversions,\
hicpp-member-init,\
hicpp-named-parameter,\
modernize-deprecated-headers,\
modernize-loop-convert,\
modernize-return-braced-init-list,\
modernize-use-nodiscard,\
modernize-use-std-print,\
modernize-use-trailing-return-type,\
performance-avoid-endl,\
performance-unnecessary-value-param,\
readability-avoid-const-params-in-decls,\
readability-braces-around-statements,\
readability-container-data-pointer,\
readability-container-size-empty,\
-readability-convert-member-functions-to-static,\
readability-else-after-return,\
readability-identifier-naming,\
readability-implicit-bool-conversion,\
readability-make-member-function-const,\
readability-redundant-member-init,\
readability-simplify-boolean-expr,\
readability-static-accessed-through-instance,\
readability-use-concise-preprocessor-directives,\
readability-use-std-min-max,\
' \
	 $(CPPFILES)

test: all
	-killall async_tcp_echo_server
	$(BUILD_DIR)/async_tcp_echo_server 8000 &
	-(echo | $(BUILD_DIR)/rrcp_async_tcp_client localhost 8000) &
	cat rrcp.txt | $(BUILD_DIR)/rrcp_async_tcp_client_threadsafe localhost 8000
	cat rrcp.txt | $(BUILD_DIR)/rrcp_async_tcp_client localhost 8000
	# NOTE: simple example only!
	# cat rrcp.txt | $(BUILD_DIR)/async_tcp_echo_client localhost 8000
	# -$(BUILD_DIR)/async_tcp_echo_client localhost
	# -echo | $(BUILD_DIR)/async_tcp_echo_client localhost 8001
	-killall async_tcp_echo_server
	ctest --test-dir $(BUILD_DIR) --rerun-failed --output-on-failure
	gcovr

format: .clang-format
	-codespell
	git ls-files ::*.cpp ::*.hpp ::*.json | xargs clang-format -i
	git ls-files ::*CMakeLists.txt | xargs gersemi -i --no-warn-about-unknown-commands
	git ls-files ::*.py | xargs black

# These rules keep make from trying to use the match-anything rule below
# to rebuild the makefiles--ouch!

CMakeLists.txt :: ;
GNUmakefile :: ;
.clang-tidy :: ;
.clang-format :: ;

# Anything we don't know how to build will use this rule.  The command is
# a do-nothing command.
% :: $(BUILD_DIR)
	ninja -C $< $@
