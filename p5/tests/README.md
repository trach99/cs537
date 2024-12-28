Running Tests
-------------

Run this script from its location
```sh
./runtests
```
It will produce `runtests.log` in this directory.
There is an example `example_output.log` in this directory.

If you want to run all the tests despite any failures, use
```sh
./runtests -c
```

To run a particular test, e.g., test 4
```sh
./runtests test_4
```

Actual usertest files are inside `ctests` directory.
Each test_1.c, test_2.c, etc., is a test case.
There is a header file `tester.h` associated with each test case.
