# Testing

There are several test suites for verifying ChromeDriver's correctness:

| Test Suite | Purpose | Frequency |
| ---------- | ------- | --------- |
| Unit tests | Quick tests for verifying individual objects or functions | CQ |
| Python integration tests | Verify important features work correctly with Chrome | CQ |
| Java acceptance tests | Verify correct interaction with Chrome and Selenium Java API | Manual runs |
| Web platform tests | Verify W3C standard complaince | CI waterfall |
| JavaScript unit tests | Verify ChromeDriver's JavaScript functions | CQ |

## Unit tests (`chromedriver_unittests`)

Many C++ source files for ChromeDriver have corresponding unit test files,
with filenames ending in `_unittest.cc`. These tests should be very quick (each
taking no more than a few milliseconds) and be very stable.
We run them on the commit queue on all desktop platforms.

Here are the commands to build and run the unit tests:
```
autoninja -C out/Default chromedriver_unittests
out/Default/chromedriver_unittests
```

## Python integration tests

These tests are maintained by the ChromeDriver team,
and are intended to verify that ChromeDriver works correctly with Chrome.
They are written in Python script, in
[`test/run_py_tests.py`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/test/chromedriver/test/run_py_tests.py).
We run these tests on the CQ (commit queue) on all desktop platforms,
and plan to run them on Android as well in the future.

To run these tests, first build Chrome and ChromeDriver, and then
invoke `run_py_tests.py`:
```
autoninja -C out/Default chrome chromedriver
python <CHROMEDRIVER_DIR>/test/run_py_tests.py --chromedriver=out/Default/chromedriver
```

The `run_py_tests.py` script has a number of options.
Run it with `--help` for more information. 
The only require option is `--chromedriver` to specify the location of
the ChromeDriver binary.

### Test filtering

The `--filter` option can be used on `run_py_tests.py` command line to filter
the tests to run. Inside the filter, tests must be specified using the format
`moduleName.className.testMethodName`, where `moduleName` is always `__main__`,
`className` is the name of the Python class containing the test,
and `testMethodName` is the Python method defining the test.
For example `--filter=__main__.ChromeDriverTest.testLoadUrl`.

The `*` character can be used inside the filter as a wildcard.

To specify multiple tests in the filter, separate them with `:` characters.

### Disabling a test

If there are any test cases that fail or are flaky, and you can't fix them
quickly, please add the test names to one of the filters near the beginning
of `run_py_tests.py`. If the failure is due to a bug, please file an issue at
https://crbug.com/chromedriver/new and include the link to the issue as a
comment. If the failure is intentional (e.g., a feature is not supported on a
particilar platform), explain it in a comment.

### Running in Commit Queue

The Python integration tests are run in the Commit Queue (CQ) in a step
named `chromedriver_py_tests`.

When running inside the CQ, the `--test-type=integration` option is passed to
the `run_py_tests.py` command line. This has the following effects:
* All tests listed in
  [`_INTEGRATION_NEGATIVE_FILTER`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/test/chromedriver/test/run_py_tests.py?q=_INTEGRATION_NEGATIVE_FILTER)
  are skipped. Tests in this list should have comments indicating why they
  should be skipped in the CQ.
* If there are a small number of test failures (no more than 10),
  then all failed tests are retried at the end.
  This is to prevent flaky tests from causing CQ failures.

## WebDriver Java acceptance tests (`test/run_java_tests.py`)

These are integration tests from the Selenium WebDriver open source project.
They are not currently run on any bots, but we have plan to include these tests
in the commit queue in the future.

The source code for these tests are in the Selenium repository at
https://github.com/SeleniumHQ/selenium/tree/master/java/client/test/org/openqa/selenium.
We compile these tests, and store them in a special repository at
https://chromium.googlesource.com/chromium/deps/webdriver/.
We use a Python script
[`test/run_java_tests.py`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/test/chromedriver/test/run_java_tests.py)
to drive these tests.

Before running these tests, you need to do a one-time setup with the following
commands:
```
mkdir <CHROMEDRIVER_DIR>/third_party
cd <CHROMEDRIVER_DIR>/third_party
git clone https://chromium.googlesource.com/chromium/deps/webdriver java_tests
```

After the setup, the tests can be run with
```
python <CHROMEDRIVER_DIR>/test/run_java_tests.py --chromedriver=out/Default/chromedriver
```

The `run_py_tests.py` script has a number of options.
Run it with `--help` for more information. 
The only require option is `--chromedriver` to specify the location of
the ChromeDriver binary.

### Disabling a test

If there are any test cases that fail or are flaky, and you can't fix them
quickly, please add the test names to one of the filters in
[`test_expectations`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/test/chromedriver/test/test_expectations)
file in the same directory as `run_java_tests.py`.

## Web Platform Tests (WPT)

The Web Platform Tests (WPT) project is a W3C-coordinated attempt to build a
cross-browser testsuit to verify how well browsers conform to web platform
standards. Here, we will only focus on the WebDriver portion of WPT.

TODO: Add details.

## JavaScript Unit Tests

All ChromeDriver JavaScript files in the
[js](https://source.chromium.org/chromium/chromium/src/+/master:chrome/test/chromedriver/js/)
directory have corresponding unit tests, stored in HTML files.
These tests can be run in two ways:

* They are run as part of the Python integration tests (`run_py_tests.py`)
  menteioned above, through a test class named `JavaScriptTests`.
  As a result of this, these tests are run in the CQ (commit queue).

* They can be run manually, by using Chrome to load the HTML file containing
  the tests. After the HTML file is loaded, open DevTools pane and switch to
  the Console tab to see the test results.
