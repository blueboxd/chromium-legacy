# WebLayer

WebLayer is a high level embedding API to support building a browser.

Unlike src/content, which is only concerned with a sandboxed multi-process web
platform, WebLayer includes modern browser features and Google integration.
It's the reusable version of Chrome, which might share some portions of the UI
and also its support for all the modern HTML5 and browser features (e.g. UI for
permissions, autofill, safe browsing etc...).

While it's built on top of src/content, the expectation is that the API will
hide the Content API.

## Design Goals
1. WebLayer should be powerful enough to build a modern browser. This means:
    * as fast and secure as Chrome
    * includes all the same web platform features as Chrome (including UIs and system integration code)
2. WebLayer embedders should get new web platform features for free (e.g. don't need to keep updating their UIs or code)
3. WebLayer hides subprocesses, so any code that needs to run in the renderer needs to be part of WebLayer. Script injection
is generally discouraged for security and performance reasons.

## Resources and Documentation

Mailing list: [weblayer-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/weblayer-dev)

Bug tracker: [Internals>WebLayer](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AInternals%3EWebLayer)

## Directory Structure

`public` the C++ and Java public API. These are the only files an app should use

`shell` sample app

'test' test harnesses and test-only helper code

'tools' helper scripts

'app' internal code which runs at the beginning of each process

`browser` internal code which runs in the browser process

`common` internal code which runs in the browser and child processes

`renderer` internal code which runs in the renderer process

`utility` internal code which runs in the utility process

## Setting Up Your Build Environment

If you haven't done this already, you first need to set up an Android build. If
you are a Google employee, reach out to weblayer-team@google.com for internal
instructions. Otherwise follow the [Android build instructions](/docs/android_build_instructions.md).

## Building and Testing

To run the sample app:

$ autoninja -C out/Default run_weblayer_shell
$ out/Default/bin/run_weblayer_shell

To run instrumentation tests:

$ autoninja -C out/Default weblayer_instrumentation_test_apk
$ out/Default/bin/run_weblayer_instrumentation_test_apk

Note: this may not work on some versions of Android. If you see an error setting
the WebView provider when running instrumentation tests, try running the tests
using the WebLayer support APK which uses a different loading path:

$ autoninja -C out/Default weblayer_support_instrumentation_test_apk
$ out/Default/bin/run_weblayer_support_instrumentation_test_apk

The test script will build and install all necessary APKs.
