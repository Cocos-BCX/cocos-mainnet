# Code coverage testing   
Check how much code is covered by unit tests, using gcov/lcov (see http://ltp.sourceforge.net/coverage/lcov.php ).
``` shell
cmake -D ENABLE_COVERAGE_TESTING=true -D CMAKE_BUILD_TYPE=Debug .
make
lcov --capture --initial --directory . --output-file base.info --no-external
libraries/fc/bloom_test
libraries/fc/task_cancel_test
libraries/fc/api
libraries/fc/blind
libraries/fc/ecc_test test
libraries/fc/real128_test
libraries/fc/lzma_test README.md
libraries/fc/ntp_test
tests/intense_test
tests/app_test
tests/chain_bench
tests/chain_test
tests/performance_test
lcov --capture --directory . --output-file test.info --no-external
lcov --add-tracefile base.info --add-tracefile test.info --output-file total.info
lcov -o interesting.info -r total.info libraries/fc/vendor/\* libraries/fc/tests/\* tests/\*
mkdir -p lcov
genhtml interesting.info --output-directory lcov --prefix `pwd`
```
Now open lcov/index.html in a browser.

# Unit testing
We use the Boost unit test framework for unit testing. Most unit tests reside in the chain_test build target.

# Witness node
The role of the witness node is to broadcast transactions, download blocks, and optionally sign them.
```shell
./witness_node --rpc-endpoint 127.0.0.1:8090 --enable-stale-production -w '"1.6.0"' '"1.6.1"' '"1.6.2"' '"1.6.3"' '"1.6.4"' '"1.6.5"' '"1.6.6"' '"1.6.7"' '"1.6.8"' '"1.6.9"' '"1.6.10"' '"1.6.11"' 
```

# Running specific tests
```shell
tests/chain_test -t block_tests/name_of_test
```

