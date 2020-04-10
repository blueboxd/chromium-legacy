# WebView DevTools user guide

[TOC]

## Launching WebView DevTools

WebView DevTools is an on-device suite of tools that ships with WebView itself.
You can launch WebView DevTools by any of the following:

### Launcher icon on pre-stable channels (preferred)

The best way to launch WebView DevTools is to [download WebView Beta, Dev, or
Canary](prerelease.md). These channels will have a launcher icon which will
launch WebView DevTools.

*** note
**Note:** the WebView DevTools icon does not appear by default on Android 7
through 9 (Nougat/Oreo/Pie). To enable the launcher icon, first [change your
WebView provider](prerelease.md#Android-7-through-9-Nougat_Oreo_Pie) and then
launch the same Chrome channel or any WebView app (ex. [WebView shell
browser](webview-shell.md), or open an email in Gmail).
***

### Launch via adb

If you have adb installed, you can connect your Android device to launch
DevTools:

```sh
adb shell am start -a "com.android.webview.SHOW_DEV_UI"
```

### Launch via WebView Shell

Newer versions of [WebView shell](webview-shell.md) have a menu option to launch
WebView DevTools. If your copy of WebView shell doesn't have this option, you
may need to rebuild it yourself.

## Crash UI

TODO(http://crbug.com/1058571): document this.

## Flag UI

While WebView supports [toggling arbitrary flags](commandline-flags.md) on
debuggable devices, we also support toggling a curated set of experimental
flags/features on production Android devices. We expose these features as part
of WebView's on-device DevTools. This is similar to Chrome's `chrome://flags`
tool.

![WebView flag UI](images/webview_flag_ui.png)

Tap the "Flags" option in the bottom navgation bar. You can scroll through the
list to find your desired feature/flag (ex. "highlight-all-webviews"), tap the
dropdown (look for "Default"), and tap "Enabled" in the dialog popup. You can
enable (or disable) as many flags as you need.

*** promo
**Tip:** enabling "highlight-all-webviews" (which tints all WebView objects
yellow) in addition to your desired flag is a great way to verify apps have
picked up WebView flags.
***

Kill and restart WebView apps so they pick up the new flags.

When you're done, open the notification tray and tap the WebView DevTools
notification to go back to the flag UI. Tap "Reset all to default" and kill and
restart WebView apps to go back to the default behavior.

### Overriding variations/Field Trials

Like Chrome, WebView supports A/B experiments and feature rollouts through
variations (AKA "field trials" or "Finch"). The flag UI can override the field
trial config, either to **enable** an experimental feature to ensure your app
works correctly, or to **disable** an experiment to determine if this is the
root cause for a WebView behavior change breaking your app. Simply tap "Enabled"
or "Disabled" in the UI; "Default" means WebView will pick up the random field
trial experiment.

If you find an experiment is the root cause for app breakage, please [file a
bug](https://bugs.chromium.org/p/chromium/issues/entry?template=Webview+Bugs),
mention which experiment, and link to your app's Play Store page for our team to
investigate.

### Accelerating field trial config download

You can also use the flag UI to download new field trial configs ("seeds") more
quickly, to verify the next seed will fix app breakage. Enable all of the
following:

* `finch-seed-expiration-age=0`
* `finch-seed-min-update-period=0`
* `finch-seed-min-download-period=0`
* `finch-seed-ignore-pending-download`

Restart your app, kill it, and restart it a second time. Your app should be
running with the latest WebView variations seed.

### Adding your flags and features to the UI

If you're intending to launch a feature in WebView or start a field trial (AKA
Finch experiment), we **highly encourage** you to [add to the
list](/android_webview/java/src/org/chromium/android_webview/common/ProductionSupportedFlagList.java)
(ex. [CL](https://crrev.com/c/2008007), [CL](https://crrev.com/c/2066144)).

Exposing your feature this way has several benefits:

- This improves the manual test process. Testers can enable your feature with a
  button click instead of an adb command.
- Typo-free: someone could mistype a flag or feature name in the commandline,
  but this UI ensures flag names are always spelled correctly.
- Because this works on production Android devices, test team can validate your
  feature on devices from other OEMs.
- You (and teammates) can dogfood your feature.
- If users or third-party app developers report bugs, this UI is the only way
  they can toggle your feature to help root-cause the regression.

### See also

- [Design doc](http://go/webview-dev-ui-flags-design) (Google-only)

## More cool stuff

### Switching WebView provider

Since Android 7 (Nougat), Android supports installing [multiple WebView
packages](prerelease.md) side by side and switching between them. After
installing the WebView channel you want, launch WebView DevTools, open the
three-dotted menu, and select "Change WebView Provider." This launches the
system UI where you can select your new WebView channel.

### Filing a bug report

*** aside
This was [added in M84](https://chromiumdash.appspot.com/commit/874adae46ba404dfec1660ef239899eef47d72bc).
***

You can file general bug reports for the WebView engineering team. Launch
WebView DevTools, open the three-dotted menu, and select "Report WebView Bug."
Similar to filing bug reports for crashes, this will open the system browser to
[our bug
tracker](https://bugs.chromium.org/p/chromium/issues/entry?template=Webview+Bugs&labels=Via-WebView-DevTools)
(requires sign-in), where you can describe the issue.

## Looking for Chrome DevTools?

You can debug WebView even more with Chrome DevTools! For detailed instructions,
check out [Remote Debugging
WebViews](https://developers.google.com/web/tools/chrome-devtools/remote-debugging/webviews).
