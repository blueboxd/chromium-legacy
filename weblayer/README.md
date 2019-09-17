# WebLayer

WebLayer is a high level embedding API to support building a browser.

Unlike src/content, which is only concerned with a sandboxed multi-process web
platform, WebLayer includes modern browser features and Google integration.
It's the reusable version of Chrome, which might share some portions of the UI
and also its support for all the modern HTML5 and browser features (e.g. UI for
permissions, autofill, safe browsing etc...).

While it's built on top of src/content, the expectation is that the API will
hide the Content API.

## Resources and Documentation

Mailing list: [weblayer-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/weblayer-dev)

Bug tracker: [Internals>WebLayer](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AInternals%3EWebLayer)

## Directory Structure

`public` the C++ and Java public API. These are the only files an app should use

`shell` sample app

`browser` internal code which runs in the browser process

`common` internal code which runs in the browser and child processes

`renderer` internal code which runs in the renderer process

## Testing

To run instrumentation tests:

$ autoninja -C out/Default weblayer_instrumentation_test_apk
$ out/Default/bin/run_weblayer_instrumentation_test_apk

The test script will build and install all necessary APKs.
