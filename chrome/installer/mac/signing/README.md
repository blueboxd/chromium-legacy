# Signing Scripts for Chrome on macOS

This directory contains Python modules that modify the Chrome application bundle
for various release channels, sign the resulting bundle, package it into
`.dmg`/`.pkg` files for distribution, and sign those resulting `.dmg`/`.pkg`
files.

## Invoking

The scripts are invoked using the driver located at
`//chrome/installer/mac/sign_chrome.py`. In order to sign a binary, a signing
identity is required. Googlers can use the [internal development
identity](https://goto.google.com/macoscerts); otherwise you can create a
[self-signed](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html)
identity.

A sample invocation to use during development would be:

    $ ninja -C out/release chrome chrome/installer/mac
    $ ./out/release/Chromium\ Packaging/sign_chrome.py --input out/release --output /tmp/signed --identity 'MacOS Developer' --development --disable-packaging

The `--disable-packaging` flag skips the creation of DMG and PKG files, which
speeds up the signing process when one is only interested in a signed .app
bundle. The `--development` flag skips over code signing requirements and checks
that do not work without the official Google signing identity.

## Chromium

The signing scripts do not work out-of-the-box with a Chromium build. Until
https://crbug.com/1021255 is fixed, in order to have a working (i.e.
launch-able), signed Chromium:

1. Edit chrome/app/app-entitlements.plist and remove the following key/value
   pairs:
   - `com.apple.application-identifier`
   - `keychain-access-groups`
   - `com.apple.developer.associated-domains.applinks.read-write`
2. Run `sign_chrome.py` as documented above.

Note that the Chromium [code sign
config](https://cs.chromium.org/chromium/src/chrome/installer/mac/signing/chromium_config.py)
only produces one Distribution to sign just the .app. An
`is_chrome_build=true` build produces several Distributions for the official
release system.

## Running Tests

Simply run the wrapper script at
`//chrome/installer/mac/signing/run_mac_signing_tests.py`.

You can pass `--coverage` or `-c` to show coverage information. To generate a
HTML coverage report and Python coverage package is available (via `pip install
coverage`), run:

    coverage3 run -m unittest discover -p '*_test.py'
    coverage3 html

## Formatting

The code is automatically formatted with YAPF. Run:

    git cl format --python
