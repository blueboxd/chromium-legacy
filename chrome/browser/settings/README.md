# Chrome for Android Settings

## Getting Started

The Android developer [Settings
guide](https://developer.android.com/guide/topics/ui/settings) is the best place
to start before contributing to Chrome for Android's settings.

## Helper Classes

Many common utility functions that are useful for developing settings screens in
Chrome for Android can be found in `//chrome/browser/settings/android`.

## Widgets

The `widget` subdirectory contains a number of extensions of AndroidX
[Preference](https://developer.android.com/reference/androidx/preference/Preference)
classes that provide Chrome-specific behavior (like Managed preferences) or
common Chrome UI components (like buttons).

The base Preference classes included in the AndroidX Preference library can also
be used directly in Chrome for Android Settings screens.
