# WebView Log Verbosifier

*** note
**Deprecated:** The Log Verbosifier app does not work on Android R.

Instead, use the **webview-verbose-logging** flag in [WebView
DevTools](/android_webview/docs/developer-ui.md) ([added in
M83](https://chromiumdash.appspot.com/commit/6f015ed47dd2e63b683c8fed6fece7a9ea16f824)).
This flag behaves exactly the same as if the Log Verbosifier app is installed,
but will be compatible with all OS levels (including Android R). The log format
is identical, so you can [search logcat](#Searching-logcat) as before.
***

WebView Log Verbosifier is an empty dummy app (in fact, it cannot be launched).
However, if this app is installed, WebView will log the active field trials and
CommandLine flags, for debugging/QA purposes. A dummy app is used because it can
be installed on any device (including user builds, where field trials are still
relevant).

## Build and install

```shell
autoninja -C out/Default webview_log_verbosifier_apk
out/Default/bin/webview_log_verbosifier_apk install
```

Alternatively, if you do not have a fully set up Chromium build
environment, you can build with gradle:

```shell
ANDROID_SDK_ROOT=/path/to/sdk ./gradlew build
```

## Searching logcat

You can `grep` the logcat like so:

```shell
adb logcat | grep -i 'Active field trial' # Field trials, one per line
adb logcat | grep -i 'WebViewCommandLine' # CommandLine switches, one per line
adb logcat | grep -iE 'Active field trial|WebViewCommandLine' # Both
```

Then just start up any WebView app.

## Uninstalling

When you're done investigating flags/field trials, you can disable the logging
by uninstalling the app:

```shell
out/Default/bin/webview_log_verbosifier_apk uninstall
```

## See also

* [How to set commandline flags in
  WebView](/android_webview/docs/commandline-flags.md)
