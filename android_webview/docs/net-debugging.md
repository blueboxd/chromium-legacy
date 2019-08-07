# Net debugging in WebView

## Net log

WebView supports the `kLogNetLog` flag to log debugging network info to a JSON
file on disk.

*** aside
For more info on commandline flags, see
[commandline-flags.md](./commandline-flags.md).
***

*** note
**Note:** this requires either a `userdebug` or `eng` Android build (you can
check with `adb shell getprop ro.build.type`). Flags cannot be enabled on
production builds of Android.
***

```shell
# Optional: set any flags of your choosing before running the script
$ build/android/adb_system_webview_command_line --enable-features=MyFeature,MyOtherFeature
Wrote command line file. Current flags (in webview-command-line):
  005d1ac915b0c7d6 (bullhead-userdebug 6.0 MDB08M 2353240 dev-keys): --enable-features=MyFeature,MyOtherFeature

# Replace "<app package name>" with your app's package name (ex. the
# WebView Shell is "org.chromium.webview_shell")
$ android_webview/tools/record_netlog.py --package="<app package name>"
Running with flags ['--enable-features=MyFeature,MyOtherFeature', '--log-net-log=netlog.json']
Netlog will start recording as soon as app starts up. Press ctrl-C to stop recording.
^C
Pulling netlog to "netlog.json"
```

Then import the JSON file into [the NetLog
viewer](https://chromium.googlesource.com/catapult/+/master/netlog_viewer/).

### Manual steps

1. Figure out the app's data directory
   ```sh
   # appPackageName is the package name of whatever app you're interested (ex.
   # WebView shell is "org.chromium.webview_shell").
   appDataDir="$(adb shell dumpsys package ${appPackageName} | grep 'dataDir=' | sed 's/^ *dataDir=//')" && \
   ```
1. Pick a name for the JSON file. This must be under the WebView folder in the
   app's data directory (ex. `jsonFile="${appDataDir}/app_webview/foo.json"`).
   **Note:** it's important this is inside the data directory, otherwise
   multiple WebView apps might try (and succeed) to write to the file
   simultaneously.
1. Kill the app, if running
1. Set the netlog flag:
   ```sh
   build/android/adb_system_webview_command_line --log-net-log=${jsonFile}
   ```
1. Restart the app. Reproduce whatever is of interest, and then kill the app
   when finished
1. Get the netlog off the device:
   ```sh
   adb pull "${appDataDir}/app_webview/${jsonFile}"
   ```
 1. Follow the step above for using the Netlog viewer
