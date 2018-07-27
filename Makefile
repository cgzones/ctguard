#!/usr/bin/make -f

DESTDIR = 

CTGUARD_FLAGS = ${FLAGS}
CTGUARD_FLAGS += -DFORTIFY_SOURCE=2 -fstack-protector-strong -Wall -Wextra -Werror=all -Werror=extra -O2
CTGUARD_FLAGS += -isystem src/external/cereal-1.2.2/include/
CTGUARD_FLAGS += -isystem src/external/dtl-master/

CTGUARD_CFLAGS = ${CTGUARD_FLAGS} ${CFLAGS} -std=c11

CTGUARD_CXXFLAGS = ${CTGUARD_FLAGS} ${CXXFLAGS} -std=c++17

CTGUARD_LDFLAGS = ${LDFLAGS} -lsqlite3 -pthread

ifndef USERMODE
	CC = clang
	CXX = clang++
	
	CTGUARD_FLAGS += 
	CTGUARD_CFLAGS += -g -Weverything
	CTGUARD_CFLAGS += -fsanitize=address -fsanitize=undefined -fsanitize=integer -fsanitize=nullability
	CTGUARD_CXXFLAGS += -g -Weverything -Wno-padded -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-disabled-macro-expansion
	CTGUARD_CXXFLAGS += -Wno-reserved-id-macro -Wno-documentation-unknown-command -Wno-documentation -Wno-return-std-move-in-c++11
	CTGUARD_CXXFLAGS += -fsanitize=address -fsanitize=undefined -fsanitize=integer -fsanitize=nullability

	V=1
endif


CCCOLOR="\033[34m"
CXXCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
	QUIET_CC      = @printf '    %b %b\n' ${CCCOLOR}CC${ENDCOLOR} ${SRCCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_CXX     = @printf '    %b %b\n' ${CXXCOLOR}CXX${ENDCOLOR} ${SRCCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_LINK    = @printf '    %b %b\n' ${LINKCOLOR}LINK${ENDCOLOR} ${BINCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_CCBIN   = @printf '    %b %b\n' ${LINKCOLOR}CC${ENDCOLOR} ${BINCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_CXXBIN  = @printf '    %b %b\n' ${LINKCOLOR}CXX${ENDCOLOR} ${BINCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_INSTALL = @printf '    %b %b\n' ${LINKCOLOR}INSTALL${ENDCOLOR} ${BINCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_RANLIB  = @printf '    %b %b\n' ${LINKCOLOR}RANLIB${ENDCOLOR} ${BINCOLOR}$@${ENDCOLOR} 1>&2;
	QUIET_NOTICE  = @printf '%b' ${MAKECOLOR} 1>&2;
	QUIET_ENDCOLOR= @printf '%b' ${ENDCOLOR} 1>&2;
endif

CTGUARD_CC     =${QUIET_CC}${CC}
CTGUARD_CXX    =${QUIET_CXX}${CXX}
CTGUARD_CXXBIN =${QUIET_CXXBIN}${CXX}
CTGUARD_LINK   =${QUIET_LINK}ar -rc
CTGUARD_RANLIB =${QUIET_RANLIB}ranlib

.PHONY: all binaries full-install install docs
all: binaries docs
binaries: ctguard-diskscan ctguard-logscan ctguard-research ctguard-intervention

.cpp.o:
	${CTGUARD_CXX} -MMD ${CTGUARD_CXXFLAGS} -c $< -o $@

deps =


####################
#### external ######
####################

.PHONY: external
external: src/external/sha2/sha2.o 
src/external/sha2/sha2.o: src/external/sha2/sha2.c src/external/sha2/sha2.h
	${CTGUARD_CC} -fPIC -Werror -Wall -Wextra -O2 -DSHA2_USE_INTTYPES_H $< -c -o $@


####################
#### libs ##########
####################
libs_c := $(wildcard src/libs/*.cpp)
libs_c += $(wildcard src/libs/xml/*.cpp)
libs_c += $(wildcard src/libs/json/*.cpp)
libs_c += $(wildcard src/libs/filesystem/*.cpp)
libs_c += $(wildcard src/libs/sqlite/*.cpp)
libs_c += $(wildcard src/libs/config/*.cpp)
libs_o := $(libs_c:.cpp=.o)
deps   += $(libs_c:.cpp=.d)


####################
#### diskscan ######
####################
diskscan_c := $(wildcard src/diskscan/*.cpp)
diskscan_o := $(diskscan_c:.cpp=.o)
deps       += $(diskscan_c:.cpp=.d)

ctguard-diskscan: ${diskscan_o} ${libs_o} src/external/sha2/sha2.o
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@


####################
#### intervention ##
####################
intervention_c := $(wildcard src/intervention/*.cpp)
intervention_o := $(intervention_c:.cpp=.o)
deps           += $(intervention_c:.cpp=.d)

ctguard-intervention: ${intervention_o} ${libs_o}
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@

####################
#### research ######
####################
research_c := $(wildcard src/research/*.cpp)
research_o := $(research_c:.cpp=.o)
deps       += $(research_c:.cpp=.d)

ctguard-research: ${research_o} ${libs_o}
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@


####################
#### logscan #######
####################
logscan_c := $(wildcard src/logscan/*.cpp)
logscan_o := $(logscan_c:.cpp=.o)
deps       += $(logscan_c:.cpp=.d)

ctguard-logscan: ${logscan_o} ${libs_o}
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@


####################
#### test ##########
####################
test_c := $(wildcard src/test/*.cpp)
test_o := $(test_c:.cpp=.o)
deps   += $(test_c:.cpp=.d)

.PHONY: tests tests-coverage
tests: runner
	./runner

tests-coverage:

	${MAKE} clean
	${MAKE} CC=gcc CXX=g++ FLAGS="-g -O0 --coverage" V=1 runner

	lcov --base-directory . --directory . --zerocounters --rc lcov_branch_coverage=1 --quiet
	@echo -e "Running tests\n"

	./runner

	@echo -e "\nTests finished."

	lcov --base-directory . --directory . --capture --no-external --quiet --rc lcov_branch_coverage=1 --output-file ctguard.test

	rm -rf coverage-report/
	genhtml --branch-coverage --output-directory coverage-report/ --title "ctguard test coverage" --show-details --legend --num-spaces 4 --quiet ctguard.test

	${MAKE} clean

test_files_h := $(wildcard src/test/test_*.h)
test_files_cc := $(test_files_h:.h=.cc)

src/test/%.cc: src/test/%.h
	cxxtestgen --part --error-printer -o $@ $^

src/test/runner.cc:
	cxxtestgen --root --error-printer -o $@

runner: src/test/runner.cc ${test_files_cc} ${libs_o} src/external/sha2/sha2.o
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} -Wconversion -Werror $^ ${CTGUARD_LDFLAGS} -o $@

logger_test: src/test/logger_test.o ${libs_o}
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@

xml_test: src/test/xml_test.o ${libs_o}
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@

sha256_test: src/test/sha256_test.o ${libs_o} src/external/sha2/sha2.o
	${CTGUARD_CXXBIN} ${CTGUARD_CXXFLAGS} $^ ${CTGUARD_LDFLAGS} -o $@


####################
#### docs ##########
####################

docs_src := $(wildcard docs/*.adoc)
docs_template_src := $(wildcard docs/*.adoc.template)
docs_man := $(docs_src:.adoc=)

docs/%: docs/%.adoc ${docs_template_src}
	asciidoctor -b manpage -v $<

docs: ${docs_man}

####################
#### install #######
####################

install: binaries
	install -d ${DESTDIR}/usr/sbin ${DESTDIR}/lib/systemd/system
	install -d -m 750 ${DESTDIR}/etc/ctguard/interventions ${DESTDIR}/etc/ctguard/rules ${DESTDIR}/var/lib/ctguard ${DESTDIR}/var/log/ctguard
	install ctguard-logscan ctguard-research ctguard-diskscan ctguard-intervention ${DESTDIR}/usr/sbin
	install -m 640 src/cfg/logscan.conf src/cfg/research.conf src/cfg/diskscan.conf src/cfg/intervention.conf ${DESTDIR}/etc/ctguard
	install -m 640 rules/*.xml ${DESTDIR}/etc/ctguard/rules
	install -m 700 src/cfg/block_ip.sh ${DESTDIR}/etc/ctguard/interventions
	install -m 644 src/cfg/ctguard-research.service src/cfg/ctguard-logscan.service src/cfg/ctguard-diskscan.service src/cfg/ctguard-intervention.service ${DESTDIR}/lib/systemd/system
	
full-install: install
	adduser --system --group --quiet --gecos "ctguard daemon" --no-create-home --home ${DESTDIR}/var/lib/ctguard ctguard

	chown root:ctguard ${DESTDIR}/etc/ctguard/research.conf ${DESTDIR}/etc/ctguard/rules ${DESTDIR}/etc/ctguard/rules/*.xml
	chown ctguard:ctguard ${DESTDIR}/var/lib/ctguard ${DESTDIR}/var/log/ctguard

	chmod o-rwx ${DESTDIR}/etc/ctguard/research.conf ${DESTDIR}/etc/ctguard/rules/*.xml ${DESTDIR}/etc/ctguard/logscan.conf ${DESTDIR}/etc/ctguard/diskscan.conf ${DESTDIR}/etc/ctguard/intervention.conf
	

####################
#### format ########
####################

format:
	find src/ -path src/external -prune -iname *.h -o -iname *.hpp -o -iname *.cpp | xargs clang-format -i

####################
#### analysis ######
####################

.PHONY: analysis analysis-cppcheck analysis-clang
analysis: analysis-cppcheck analysis-clang

analysis-cppcheck:
	cppcheck --enable=all --error-exitcode=2 --force --inconclusive -j 4 --library=std.cfg --library=posix.cfg --std=c++14 --std=posix src/ --quiet

analysis-clang:
	${MAKE} clean
	${MAKE} external
	scan-build -analyze-headers --status-bugs --use-cc=clang --use-c++=clang++ ${MAKE}


####################
#### clean #########
####################

.PHONY: clean clean-test clean-coverage-report
clean: clean-test
	rm -f src/external/sha2/sha2.o
	rm -f ${libs_o} libs.a
	rm -f ${diskscan_o} ctguard-diskscan
	rm -f ${intervention_o} ctguard-intervention
	rm -f ${logscan_o} ctguard-logscan
	rm -f ${research_o} ctguard-research
	rm -f ${docs_man}
	rm -f ${deps}

clean-test:
	rm -f ${test_o} thread_test logger_test xml_test sha256_test
	rm -f runner src/test/runner.cc src/test/*.cc
	find . -name '*.gcda' -exec rm {} \;
	find . -name '*.gcno' -exec rm {} \;
	rm -f ctguard.test

clean-coverage-report:
	rm -Rf coverage-report

-include ${deps}
