# System WebView Shell

WebView team maintains a "shell"--a thin interface over the WebView APIs--to
exercise WebView functionality. The System WebView Shell (AKA "shell browser,"
"WebView shell") is a standalone app implemented [in
chromium](/android_webview/tools/system_webview_shell/). While often used for
manual testing, we also use the shell for automated tests (see our [layout and
page cycler tests](./test-instructions.md#layout-tests-and-page-cycler-tests)).

*** note
This relies on the WebView installed on the system. So if you're trying to
verify local changes to WebView, or run against a specific WebView build, you
must **install WebView first.**
***

*** promo
**Tip:** the shell displays the WebView version (the corresponding [chromium version
number](https://www.chromium.org/developers/version-numbers)) in the title bar
at the top. This can be helpful for checking which WebView version is installed
& selected on the device.
***

## Setting up the build

The bare minimum GN args is just `target_os = "android"`. It's simplest to just
reuse the same out/ folder you use for WebView or Chrome for Android.

If you're building for an emulator, be aware WebView shell is preinstalled with
a different signing key. If you just need a WebView app, the preinstalled
WebView shell may be sufficient (and you don't need to build your own).

If you want a more up-to-date WebView shell or you want to use the convenience
scripts in this guide (ex. `.../system_webview_shell_apk launch <url>`), then
you can workaround the signature mismatch by changing your local WebView shell's
package name. Simply add the following to your GN args (run `gn args
out/Default`):

```gn
# Change the package name to anything that won't conflict. If you're not sure
# what to use, here's a safe choice:
system_webview_shell_package_name = "org.chromium.my_webview_shell"
```

This will let your local build install alongside the preinstalled WebView shell.
If you'd like, you can disable the preinstalled shell to avoid confusing the two
apps. In a terminal:

```sh
$ adb root
# Make sure to specify the default package name ("org.chromium.webview_shell"),
# not the one you used in the GN args above!
$ adb shell pm disable org.chromium.webview_shell
```

## Building the shell

```sh
$ autoninja -C out/Default system_webview_shell_apk
```

## Installing the shell

```sh
# Build and install
$ out/Default/bin/system_webview_shell_apk install
```

## Running the shell

```sh
# Launch a URL from the commandline, or open the app from the app launcher
$ out/Default/bin/system_webview_shell_apk launch "https://www.google.com/"

# For more commands:
$ out/Default/bin/system_webview_shell_apk --help
```

*** note
**Note:** `system_webview_shell_apk` does not support modifying CLI flags. See
https://crbug.com/959425. Instead, you should modify WebView's flags by
following [commandline-flags.md](./commandline-flags.md).
***

## Troubleshooting

### INSTALL\_FAILED\_UPDATE\_INCOMPATIBLE: Package ... signatures do not match previously installed version

The easiest way to workaround this is to [change the shell's package name in a
local build](#building-for-the-emulator).

If you **need** to use the same package name (ex. you're installing an official
build of WebView shell), then you can modify the system image.

*** note
**Note:** If using the emulator ensure it is being started with the
`-writable-system` option as per the
[Writable system partition](/docs/android_emulator.md#writable-system-partition)
instructions.
***

```sh
# Remount the /system partition read-write
$ adb root
$ adb remount
# Get the APK path to the WebView shell
$ adb shell pm path org.chromium.webview_shell
package:/system/app/Browser2/Browser2.apk
# Use the APK path above to delete the APK
$ adb shell rm /system/app/Browser2/Browser2.apk
# Restart the Android shell to "forget" about the WebView shell
$ adb shell stop
$ adb shell start
```
