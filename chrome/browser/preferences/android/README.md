# Storing preferences

This guide is intended for developers of Chrome for Android who need to read
and/or write small amounts of data from Java to a persistent key-value store.

## SharedPreferencesManager

[`SharedPreferencesManager`][0] is a lightweight wrapper around Android
[SharedPreferences][1] to handle additional key management logic in Chrome. It
supports reading and writing simple key-value pairs to a file that is saved
across app sessions.

## PrefServiceBridge

[`PrefServiceBridge`][2] is a JNI bridge providing access to the native Chrome
[PrefService][3] instance associated with the active user profile. This
interface can be used to read and write prefs once they're registered through
the `PrefRegistry` and added to the [`@Pref` enum][4].

## FAQ

**Should I use SharedPreferences or PrefService?**

Ask yourself the following questions about the preference to be stored:

* Will the preference need to be accessed from native C++ code?
* Should the preference be configured as syncable, so that its state can be
  managed by Chrome Sync at Backup and Restore?
* Does the preference need a managed policy setting?

If the answer to one or more of the above questions is Yes, then the preference
should be stored in PrefService. If the answer to all of the above questions is
No, then SharedPreferences should be preferred.

**What if the PrefService type I need to access is not supported by
PrefServiceBridge (i.e. double, Time, etc.)?**

If a base value type is supported by PrefService, then PrefServiceBridge should
be extended to support it once it's needed.

**How do I access a PrefService pref associated with local state rather than
browser profile?**

Most Chrome for Android preferences should be associated with a specific
profile. If your preference should instead be associated with [local state][6]
(for example, if it is related to the First Run Experience), then you should not
use PrefServiceBridge and should instead create your own feature-specific JNI
bridge to access the correct PrefService instance (see
[`first_run_utils.cc`][7]).

[0]: https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/preferences/android/java/src/org/chromium/chrome/browser/preferences/SharedPreferencesManager.java
[1]: https://developer.android.com/reference/android/content/SharedPreferences
[2]: https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/preferences/android/java/src/org/chromium/chrome/browser/preferences/PrefServiceBridge.java
[3]: https://chromium.googlesource.com/chromium/src/+/master/services/preferences/README.md
[4]: https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/android/preferences/prefs.h
[5]: https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/preferences/OWNERS
[6]: https://www.chromium.org/developers/design-documents/preferences#TOC-Introduction
[7]: https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/first_run/android/first_run_utils.cc
