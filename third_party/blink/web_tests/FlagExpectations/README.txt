FlagExpectations stores flag-specific test expectations.  To run layout tests
with a flag, use:

  run_web_tests.py --additional-driver-flag=--name-of-flag

Create a new file:

  FlagExpectations/name-of-flag

These are formatted:
  crbug.com/123456 path/to/your/test.html [ Expectation ]

Then run the tests with --additional-expectations:

  run_web_tests.py --additional-driver-flag=--name-of-flag
  --additional-expectations=path/to/FlagExpectations/name-of-flag

which will override the main TestExpectations file.

When passing a set of tests via the command line, such as using --test-list,
the SKIP expectation is not respected by default. The option --skipped=always
can be added in order to actually skip those tests.

To run a subset of all tests, with custom expectations, and to properly skip:
  run_web_tests.py --additional-driver-flag=--name-of-flag
  --additional-expectations=path/to/FlagExpectations/name-of-flag
  --test-list=path/to/test-list-file --skipped=always
