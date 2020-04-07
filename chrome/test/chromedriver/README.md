# ChromeDriver

This file contains high-level info about how ChromeDriver works and how to
contribute. If you are looking for information on how to use ChromeDriver,
please see the [ChromeDriver user site](https://chromedriver.chromium.org/).

ChromeDriver is an implementation of the
[WebDriver standard](https://w3c.github.io/webdriver/),
which allows users to automate testing of their website across browsers.

## Getting Started

Build ChromeDriver by building the `chromedriver` target, e.g.,

```
ninja -C out/Default chromedriver
```

This will create an executable binary in the build folder named
`chromedriver[.exe]`.

Once built, ChromeDriver can be used with various third-party libraries that
support WebDriver protocol, including language bindings provided by Selenium.

For testing purposes, ChromeDriver can be used interactively with python.
The following code uses our own testing API, not the commonly used Python
binding provided by Selenium.

```python
$ export PYTHONPATH=<THIS_DIR>/server:<THIS_DIR>/client
$ python
>>> import server
>>> import chromedriver
>>> cd_server = server.Server('/path/to/chromedriver/executable')
>>> driver = chromedriver.ChromeDriver(cd_server.GetUrl())
>>> driver.Load('http://www.google.com')
>>> driver.Quit()
>>> cd_server.Kill()
```

By default, ChromeDriver will look in its own directory for Chrome to use.
If Chrome is not found there, it will use the system installed Chrome.

To use ChromeDriver with Chrome on Android pass the Android package name in the
chromeOptions.androidPackage capability when creating the driver. The path to
adb_commands.py and the adb tool from the Android SDK must be set in PATH. For
more detailed instructions see the user site.

## Architecture

ChromeDriver is shipped separately from Chrome. It controls Chrome out of
process through [DevTools](https://chromedevtools.github.io/devtools-protocol/).
ChromeDriver is a standalone server which
communicates with the WebDriver client via the WebDriver wire protocol, which
is essentially synchronous JSON commands over HTTP. WebDriver clients are
available in many languages, and many are available from the open source
[Selenium WebDriver project](https://www.selenium.dev/).
ChromeDriver uses the webserver from
[net/server](https://source.chromium.org/chromium/chromium/src/+/master:net/server/).

ChromeDriver has a main thread, called the command thread, an IO thread,
and a thread per session. The webserver receives a request on the IO thread,
which is sent to a handler on the command thread. The handler executes the
appropriate command function, which completes asynchronously. The create
session command may create a new thread for subsequent session-related commands,
which will execute on the dedicated session thread synchronously. When a
command is finished, it will invoke a callback, which will eventually make its
way back to the IO thread as a HTTP response for the server to send.

## Code structure (relative to this file)

* [(this directory)](.):
Implements chromedriver commands.

* [chrome/](chrome/):
A basic interface for controlling Chrome. Should not depend on or reference
WebDriver-related code or concepts.

* [js/](js/):
Javascript helper scripts.

* [net/](net/):
Code to deal with network communication, such as connection to DevTools.

* [client/](client/):
Code for a python client.

* [server/](server/):
Code for the chromedriver server.
A python wrapper to the chromedriver server.

* [extension/](extension/):
An extension used for automating the desktop browser.

* [test/](test/):
Integration tests.

## Testing

There are several test suites for verifying ChromeDriver's correctness:

### chromedriver_unittests

This is the unittest target, which runs on the commit queue on win/mac/linux.
Tests should take a few milliseconds and be very stable.

```
ninja -C out/Default chromedriver_unittests
out/Default/chromedriver_unittests
```

### python integration tests (`test/run_py_tests.py`)

These tests are maintained by the ChromeDriver team, and are intended to verify
that ChromeDriver works correctly with Chrome. Run test/run_py_tests.py --help
for more info. These are run on the commit queue on win/mac/linux.

```
ninja -C out/Default chrome chromedriver
<THIS_DIR>/test/run_py_tests.py --chromedriver=out/Default/chromedriver
```

### WebDriver Java acceptance tests (`test/run_java_tests.py`)

These are integration tests from the WebDriver open source project which can
be run via test/run_java_tests.py. They are not currently run on any bots, but
will be included in the commit queue in the future. Run with --help for more
info.

## Contributing

Find an open issue and submit a patch for review by an individual listed in
the `OWNERS` file in this directory. Issues are tracked in
[ChromeDriver issue tracker](https://crbug.com/chromedriver).
