default:
	scons
clean:
	scons -c
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcda.info
	rm -f *.gcno

test: default
	./test_amp

coverage:
	BUILD_COVERAGE_SUPPORT=1 scons # build with coverage profiling support

	# remove existing files so results from previous runs don't accumulate
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcda.info

	# run tests to generate raw coverage files
	./test_amp

	COMPILE_COVERAGE=1 scons # compile raw coverage files to human-readable text

valgrind: default
	CK_FORK=no valgrind --leak-check=full ./test_amp

debug: default
	scons -Q debug=1
	#CK_FORK=no gdb --args ./test_amp

stats:
	zsh code_stats.zsh

libjansson:
	# build bundled jansson library?
	cd jansson && [ ! -f ./configure ] && autoreconf -i || true
	cd jansson && ./configure --prefix=`pwd`/../tmp-deps
	cd jansson && make
	cd jansson && make install

