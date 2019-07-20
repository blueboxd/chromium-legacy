# Dynamic Feature Modules (DFMs)

[Android App bundles and Dynamic Feature Modules (DFMs)](https://developer.android.com/guide/app-bundle)
is a Play Store feature that allows delivering pieces of an app when they are
needed rather than at install time. We use DFMs to modularize Chrome and make
Chrome's install size smaller.

[TOC]


## Limitations

Currently (March 2019), DFMs have the following limitations:

* **WebView:** We don't support DFMs for WebView. If your feature is used by
  WebView you cannot put it into a DFM. See
  [crbug/949717](https://bugs.chromium.org/p/chromium/issues/detail?id=949717)
  for progress.
* **Android K:** DFMs are based on split APKs, a feature introduced in Android
  L. Therefore, we don't support DFMs on Android K. As a workaround
  you can add your feature to the Android K APK build. See
  [crbug/881354](https://bugs.chromium.org/p/chromium/issues/detail?id=881354)
  for progress.
* **Native Code:** We cannot move native Chrome code into a DFM. See
  [crbug/874564](https://bugs.chromium.org/p/chromium/issues/detail?id=874564)
  for progress.

## Getting started

This guide walks you through the steps to create a DFM called _Foo_ and add it
to the Chrome bundles.

*** note
**Note:** To make your own module you'll essentially have to replace every
instance of `foo`/`Foo`/`FOO` with `your_feature_name`/`YourFeatureName`/
`YOUR_FEATURE_NAME`.
***


### Create DFM target

DFMs are APKs. They have a manifest and can contain Java and native code as well
as resources. This section walks you through creating the module target in our
build system.

First, create the file `//chrome/android/features/foo/java/AndroidManifest.xml`
and add:

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:dist="http://schemas.android.com/apk/distribution"
    featureSplit="foo">

    <!-- dist:onDemand="true" makes this a separately installed module.
         dist:onDemand="false" would always install the module alongside the
         rest of Chrome. -->
    <dist:module
        dist:onDemand="true"
        dist:title="@string/foo_module_title">
        <!-- This will prevent the module to become part of the Android K
             build in case we ever want to use bundles on Android K. -->
        <dist:fusing dist:include="false" />
    </dist:module>

    <!-- Remove android:hasCode="false" when adding Java code. -->
    <application android:hasCode="false" />
</manifest>
```

Then, add a package ID for Foo so that Foo's resources have unique identifiers.
For this, add a new ID to
`//chrome/android/modules/chrome_feature_modules.gni`:

```gn
resource_packages_id_mapping = [
  ...,
  "foo=0x{XX}", # Set {XX} to next lower hex number.
]
```

Next, create a descriptor configuring the Foo module. To do this, create
`//chrome/android/features/foo/foo_module.gni` and add the following:

```gn
foo_module_desc = {
  name = "foo"
  manifest = "//chrome/android/features/foo/java/AndroidManifest.xml"
}
```

Then, add the module descriptor to the appropriate descriptor list in
//chrome/android/modules/chrome_feature_modules.gni, e.g. the Chrome Modern
list:

```gn
import("//chrome/android/features/foo/foo_module.gni")
...
chrome_modern_module_descs += [ foo_module_desc ]
```

The next step is to add Foo to the list of feature modules for UMA recording.
For this, add `foo` to the `AndroidFeatureModuleName` in
`//tools/metrics/histograms/histograms.xml`:

```xml
<histogram_suffixes name="AndroidFeatureModuleName" ...>
  ...
  <suffix name="foo" label="Super Duper Foo Module" />
  ...
</histogram_suffixes>
```

<!--- TODO(tiborg): Add info about install UI. -->
Lastly, give your module a title that Chrome and Play can use for the install
UI. To do this, add a string to
`//chrome/android/java/strings/android_chrome_strings.grd`:

```xml
...
<message name="IDS_FOO_MODULE_TITLE"
  desc="Text shown when the Foo module is referenced in install start, success,
        failure UI (e.g. in IDS_MODULE_INSTALL_START_TEXT, which will expand to
        'Installing Foo for Chrome…').">
  Foo
</message>
...
```

*** note
**Note:** This is for module title only. Other strings specific to the module
should go in the module, not here (in the base module).
***

Congrats! You added the DFM Foo to Monochrome. That is a big step but not very
useful so far. In the next sections you'll learn how to add code and resources
to it.


### Building and installing modules

Before we are going to jump into adding content to Foo, let's take a look on how
to build and deploy the Monochrome bundle with the Foo DFM. The remainder of
this guide assumes the environment variable `OUTDIR` is set to a properly
configured GN build directory (e.g. `out/Debug`).

To build and install the Monochrome bundle to your connected device, run:

```shell
$ autoninja -C $OUTDIR monochrome_public_bundle
$ $OUTDIR/bin/monochrome_public_bundle install -m base -m foo
```

This will install Foo alongside the rest of Chrome. The rest of Chrome is called
_base_ module in the bundle world. The Base module will always be put on the
device when initially installing Chrome.

*** note
**Note:** You have to specify `-m base` here to make it explicit which modules
will be installed. If you only specify `-m foo` the command will fail. It is
also possible to specify no modules. In that case, the script will install the
set of modules that the Play Store would install when first installing Chrome.
That may be different than just specifying `-m base` if we have non-on-demand
modules.
***

You can then check that the install worked with:

```shell
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en, foo]
```

Then try installing the Monochrome bundle without your module and print the
installed modules:

```shell
$ $OUTDIR/bin/monochrome_public_bundle install -m base
$ adb shell dumpsys package org.chromium.chrome | grep splits
>   splits=[base, config.en]
```


### Adding java code

To make Foo useful, let's add some Java code to it. This section will walk you
through the required steps.

First, define a module interface for Foo. This is accomplished by adding the
`@ModuleInterface` annotation to the Foo interface. This annotation
automatically creates a `FooModule` class that can be used later to install and
access the module. To do this, add the following in the new file
`//chrome/android/features/foo/public/java/src/org/chromium/chrome/features/foo/Foo.java`:

```java
package org.chromium.chrome.features.foo;

import org.chromium.components.module_installer.ModuleInterface;

/** Interface to call into Foo feature. */
@ModuleInterface(module = "foo", impl = "org.chromium.chrome.features.FooImpl")
public interface Foo {
    /** Magical function. */
    void bar();
}
```

*** note
**Note:** To reflect the separation from "Chrome browser" code, features should
be defined in their own package name, distinct from the chrome package - i.e.
`org.chromium.chrome.features.<feature-name>`.
***

Next, define an implementation that goes into the module in the new file
`//chrome/android/features/foo/java/src/org/chromium/chrome/features/foo/FooImpl.java`:

```java
package org.chromium.chrome.features.foo;

import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;

@UsedByReflection("FooModule")
public class FooImpl implements Foo {
    @Override
    public void bar() {
        Log.i("FOO", "bar in module");
    }
}
```

You can then use this provider to access the module if it is installed. To test
that, instantiate Foo and call `bar()` somewhere in Chrome:

```java
if (FooModule.isInstalled()) {
    FooModule.getImpl().bar();
} else {
    Log.i("FOO", "module not installed");
}
```

The interface has to be available regardless of whether the Foo DFM is present.
Therefore, put those classes into the base module. For this create a list of
those Java files in
`//chrome/android/features/foo/public/foo_public_java_sources.gni`:

```gn
foo_public_java_sources = [
  "//chrome/android/features/foo/public/java/src/org/chromium/chrome/features/foo/Foo.java",
]
```

Then add this list to `chrome_java in //chrome/android/BUILD.gn`:

```gn
...
import("modules/foo/public/foo_public_java_sources.gni")
...
android_library("chrome_java") {
  ...
  java_files += foo_public_java_sources
}
...
```

The actual implementation, however, should go into the Foo DFM. For this
purpose, create a new file `//chrome/android/features/foo/BUILD.gn` and make a
library with the module Java code in it:

```gn
import("//build/config/android/rules.gni")

android_library("java") {
  # Define like ordinary Java Android library.
  java_files = [
    "java/src/org/chromium/chrome/features/foo/FooImpl.java",
    # Add other Java classes that should go into the Foo DFM here.
  ]
  # Put other Chrome libs into the classpath so that you can call into the rest
  # of Chrome from the Foo DFM.
  deps = [
    "//base:base_java",
    "//chrome/android:chrome_java",
    # etc.
    # Also, you'll need to depend on any //third_party or //components code you
    # are using in the module code.
  ]
}
```

Then, add this new library as a dependency of the Foo module descriptor in
`//chrome/android/features/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  java_deps = [
    "//chrome/android/features/foo:java",
  ]
}
```

Finally, tell Android that your module is now containing code. Do that by
removing the `android:hasCode="false"` attribute from the `<application>` tag in
`//chrome/android/features/foo/java/AndroidManifest.xml`. You should be left
with an empty tag like so:

```xml
...
    <application />
...
```

Rebuild and install `monochrome_public_bundle`. Start Chrome and run through a
flow that tries to executes `bar()`. Depending on whether you installed your
module (`-m foo`) "`bar in module`" or "`module not installed`" is printed to
logcat. Yay!


### Adding native code

Coming soon (
[crbug/874564](https://bugs.chromium.org/p/chromium/issues/detail?id=874564)).

You can already add third party native code or native Chrome code that has no
dependency on other Chrome code. To add such code add it as a loadable module to
the module descriptor in `//chrome/android/features/foo/foo_module.gni`:

```gn
foo_module_desc = {
  ...
  loadable_modules_32_bit = [ "//path/to/32/bit/lib.so" ]
  loadable_modules_64_bit = [ "//path/to/64/bit/lib.so" ]
}
```


### Adding android resources

In this section we will add the required build targets to add Android resources
to the Foo DFM.

First, add a resources target to `//chrome/android/features/foo/BUILD.gn` and
add it as a dependency on Foo's `java` target in the same file:

```gn
...
android_resources("java_resources") {
  # Define like ordinary Android resources target.
  ...
  custom_package = "org.chromium.chrome.features.foo"
}
...
android_library("java") {
  ...
  deps = [
    ":java_resources",
  ]
}
```

To add strings follow steps
[here](http://dev.chromium.org/developers/design-documents/ui-localization) to
add new Java GRD file. Then create
`//chrome/android/features/foo/java/strings/android_foo_strings.grd` as follows:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<grit
    current_release="1"
    latest_public_release="0"
    output_all_resource_defines="false">
  <outputs>
    <output
        filename="values-am/android_foo_strings.xml"
        lang="am"
        type="android" />
    <!-- List output file for all other supported languages. See
         //chrome/android/java/strings/android_chrome_strings.grd for the full
         list. -->
    ...
  </outputs>
  <translations>
    <file lang="am" path="vr_translations/android_foo_strings_am.xtb" />
    <!-- Here, too, list XTB files for all other supported languages. -->
    ...
  </translations>
  <release allow_pseudo="false" seq="1">
    <messages fallback_to_english="true">
      <message name="IDS_BAR_IMPL_TEXT" desc="Magical string.">
        impl
      </message>
    </messages>
  </release>
</grit>
```

Then, create a new GRD target and add it as a dependency on `java_resources` in
`//chrome/android/features/foo/BUILD.gn`:

```gn
...
java_strings_grd("java_strings_grd") {
  defines = chrome_grit_defines
  grd_file = "java/strings/android_foo_strings.grd"
  outputs = [
    "values-am/android_foo_strings.xml",
    # Here, too, list output files for other supported languages.
    ...
  ]
}
...
android_resources("java_resources") {
  ...
  deps = [":java_strings_grd"]
  custom_package = "org.chromium.chrome.features.foo"
}
...
```

You can then access Foo's resources using the
`org.chromium.chrome.features.foo.R` class. To do this change
`//chrome/android/features/foo/java/src/org/chromium/chrome/features/foo/FooImpl.java`
to:

```java
package org.chromium.chrome.features.foo;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.features.foo.R;

@UsedByReflection("FooModule")
public class FooImpl implements Foo {
    @Override
    public void bar() {
        Log.i("FOO", ContextUtils.getApplicationContext().getString(
                R.string.bar_impl_text));
    }
}
```

*** note
**Warning:** While your module is emulated (see [below](#on-demand-install))
your resources are only available through
`ContextUtils.getApplicationContext()`. Not through activities, etc. We
therefore recommend that you only access DFM resources this way. See
[crbug/949729](https://bugs.chromium.org/p/chromium/issues/detail?id=949729)
for progress on making this more robust.
***


### Module install

So far, we have installed the Foo DFM as a true split (`-m foo` option on the
install script). In production, however, we have to explicitly install the Foo
DFM for users to get it. There are two install options: _on-demand_ and
_deferred_.


#### On-demand install

On-demand requesting a module will try to download and install the
module as soon as possible regardless of whether the user is on a metered
connection or whether they have turned updates off in the Play Store app.

You can use the autogenerated module class to on-demand install the module like
so:

```java
FooModule.install((success) -> {
    if (success) {
        FooModule.getImpl().bar();
    }
});
```

**Optionally**, you can show UI telling the user about the install flow. For
this, add a function like the one below. Note, it is possible
to only show either one of the  install, failure and success UI or any
combination of the three.

```java
public static void installModuleWithUi(
        Tab tab, OnModuleInstallFinishedListener onFinishedListener) {
    ModuleInstallUi ui =
            new ModuleInstallUi(
                    tab,
                    R.string.foo_module_title,
                    new ModuleInstallUi.FailureUiListener() {
                        @Override
                        public void onRetry() {
                            installModuleWithUi(tab, onFinishedListener);
                        }

                        @Override
                        public void onCancel() {
                            onFinishedListener.onFinished(false);
                        }
                    });
    // At the time of writing, shows toast informing user about install start.
    ui.showInstallStartUi();
    FooModule.install(
            (success) -> {
                if (!success) {
                    // At the time of writing, shows infobar allowing user
                    // to retry install.
                    ui.showInstallFailureUi();
                    return;
                }
                // At the time of writing, shows toast informing user about
                // install success.
                ui.showInstallSuccessUi();
                onFinishedListener.onFinished(true);
            });
}
```

To test on-demand install, "fake-install" the DFM. It's fake because
the DFM is not installed as a true split. Instead it will be emulated by Chrome.
Fake-install and launch Chrome with the following command:

```shell
$ $OUTDIR/bin/monochrome_public_bundle install -m base -f foo
$ $OUTDIR/bin/monochrome_public_bundle launch --args="--fake-feature-module-install"
```

When running the install code, the Foo DFM module will be emulated.
This will be the case in production right after installing the module. Emulation
will last until Play Store has a chance to install your module as a true split.
This usually takes about a day.

*** note
**Warning:** There are subtle differences between emulating a module and
installing it as a true split. We therefore recommend that you always test both
install methods.
***


#### Deferred install

Deferred install means that the DFM is installed in the background when the
device is on an unmetered connection and charging. The DFM will only be
available after Chrome restarts. When deferred installing a module it will
not be faked installed.

To defer install Foo do the following:

```java
FooModule.installDeferred();
```


### Integration test APK and Android K support

On Android K we still ship an APK. To make the Foo feature available on Android
K add its code to the APK build. For this, add the `java` target to
the `chrome_public_common_apk_or_module_tmpl` in
`//chrome/android/chrome_public_apk_tmpl.gni` like so:

```gn
template("chrome_public_common_apk_or_module_tmpl") {
  ...
  target(_target_type, target_name) {
    ...
    if (_target_type != "android_app_bundle_module") {
      deps += [
        "//chrome/android/features/foo:java",
      ]
    }
  }
}
```

This will also add Foo's Java to the integration test APK. You may also have to
add `java` as a dependency of `chrome_test_java` if you want to call into Foo
from test code.
