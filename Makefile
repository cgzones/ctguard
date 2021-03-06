#!/usr/bin/make -f

.PHONY: default clang-tidy clean clean-light format cppcheck


default:
	cmake -B build_default/ -G Ninja -Wdeprecated --warn-uninitialized
	ninja -C build_default/

test-default: default
	CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=20 ninja -C build_default/ test


sanitizers:
	CC=clang-11 CXX=clang++-11 cmake -B build_sanitizers/ -G Ninja -Wdeprecated --warn-uninitialized -DENABLE_OPTIMIZATIONS=OFF -DENABLE_SANITIZERS=ON -DENABLE_DOC=OFF
	CC=clang-11 CXX=clang++-11 ninja -C build_sanitizers/
	ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1 CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=20 ninja -C build_sanitizers/ test


clang-tidy:
	CC=clang CXX=clang++ cmake -B build_clang_tidy/ -G Ninja -Wdeprecated --warn-uninitialized -DRUN_CLANG_TIDY=ON
	CC=clang CXX=clang++ ninja -C build_clang_tidy/


clean: clean-deb
	rm -Rf build_default/
	rm -Rf build_clang_tidy/
	rm -Rf build_sanitizers/


clean-light:
	-ninja -C build_default/ clean
	-ninja -C build_clang_tidy/ clean
	-ninja -C build_sanitizers/ clean


clean-deb:
	rm -Rf debian/.debhelper/ debian/ctguard/ obj-x86_64-linux-gnu/
	rm -f debian/ctguard.debhelper.log debian/ctguard.postrm.debhelper debian/ctguard.substvars debian/debhelper-build-stamp debian/files


format:
	find src/ -path src/external -prune -o \( -iname '*.h' -o -iname '*.hpp' -o -iname '*.cpp' \) -execdir clang-format-11 -i --Werror {} \;


cppcheck:
	cppcheck --enable=all --error-exitcode=2 --force --inconclusive -j 4 --library=std.cfg --library=posix.cfg --std=c++17 --quiet --inline-suppr --template=cppcheck1 -i src/external/ src/
