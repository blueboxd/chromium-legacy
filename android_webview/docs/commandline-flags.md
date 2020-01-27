# Commandline flags

## Applying flags

*** note
**Note:** this requires either a `userdebug` or `eng` Android build (you can
check with `adb shell getprop ro.build.type`). Flags cannot be enabled on
production builds of Android.
***

WebView reads flags from a specific file on the device as part of the startup
sequence. Therefore, it's important to always **kill the WebView-based app**
you're examining after modifying commandline flags, to ensure the flags are
picked up during the next app restart.

WebView always looks for the same file on the device
(`/data/local/tmp/webview-command-line`), regardless of which package is the
[the WebView provider](prerelease.md).

### Python script

The simplest way to set WebView flags is with the dedicated python script. This
works regardless of which package is the WebView provider:

```sh
# Overwrite flags (supports multiple)
build/android/adb_system_webview_command_line --show-composited-layer-borders --force-enable-metrics-reporting
# Clear flags
build/android/adb_system_webview_command_line ""
# Print flags
build/android/adb_system_webview_command_line
```

### Generated Wrapper Script

If you have a locally compiled APK, you may instead set flags using the
Generated Wrapper Script like so:

```sh
autoninja -C out/Default system_webview_apk
# Overwrite flags (supports multiple)
out/Default/bin/system_webview_apk argv --args='--show-composited-layer-borders --force-enable-metrics-reporting'
# Clear flags
out/Default/bin/system_webview_apk argv --args=''
# Print flags
out/Default/bin/system_webview_apk argv
```

*** note
**Note:** be careful if using a `monochrome_*` target, as the Generated Wrapper
Script writes to Chrome browser's flags file, and WebView **will not pick up
these flags**. If using Monochrome, you can set flags with the
`system_webview_*` Generated Wrapper Scripts, or use one of the other methods
in this doc.
***

### Manual

Or, you can use the `adb` in your `$PATH` like so:

```sh
FLAG_FILE=/data/local/tmp/webview-command-line
# Overwrite flags (supports multiple). The first token is ignored. We use '_'
# as a convenient placeholder, but any token is acceptable.
adb shell "echo '_ --show-composited-layer-borders --force-enable-metrics-reporting' > ${FLAG_FILE}"
# Clear flags
adb shell "rm ${FLAG_FILE}"
# Print flags
adb shell "cat ${FLAG_FILE}"
```

## Verifying flags are applied

You can confirm you've applied commandline flags correctly by dumping the full
state of the commandline flags with the [WebView Log Verbosifier
app](/android_webview/tools/webview_log_verbosifier/README.md) and starting up a
WebView app.

## Applying Features with flags

[`base::Feature`s](/base/feature_list.h) (or, "Features") are Chromium's
mechanism for toggling off-by-default code paths. While debugging flags are also
off-by-default, Features typically guard soon-to-launch product enhancements
until they're tested enough for field trials or public launch, at which point
the Feature is removed and the legacy code path is no longer supported and
removed from the codebase. On the other hand, debugging flags don't "launch," as
they're typically only helpful for debugging issues.

WebView supports the same syntax for toggling Features as the rest of chromium:
`--enable-features=feature1,feature2` and
`--disable-features=feature3,feature4`. You can apply `--enable-features` and
`--disable-features` like any other flags, per the steps above. Please consult
[`base/feature_list.h`](/base/feature_list.h) for details.

## Finding Features and flags

WebView supports toggling any flags/Features supported in any layer we
depend on (ex. content). For more details on Chromium's layer architecture, see
[this diagram](https://www.chromium.org/developers/content-module) (replace
"chrome" with "android\_webview"). Although we support toggling these flags, not
all flags will have an effect when toggled, nor do we guarantee WebView
functions correctly when the flag is toggled.

Some interesting flags and Features:

 * `--show-composited-layer-borders`: highlight rendering layers, which is
   useful for identifying which content in the app is rendered by a WebView.
 * `--force-enable-metrics-reporting`: enable UMA metrics reporting (does not
   override app opt-out)
 * `--finch-seed-expiration-age=0 --finch-seed-min-update-period=0 --finch-seed-min-download-period=0 --finch-seed-ignore-pending-download`: always request a new finch seed when an app starts

WebView also defines its own flags and Features:

 * [AwSwitches.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/common/AwSwitches.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/common/aw_switches.h))
 * [AwFeatureList.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/AwFeatureList.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/common/aw_features.h))

## Implementation

See [CommandLineUtil.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/common/CommandLineUtil.java).
