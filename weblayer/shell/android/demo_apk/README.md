# WebLayer Shell

This directory contains a demo app to demonstrate capabilities of WebLayer.

To build and run on Android:
```
autoninja -C out/Default weblayer_support_apk weblayer_demo_apk
out/Default/bin/weblayer_support_apk install
out/Default/bin/weblayer_demo_apk install
adb shell am start org.chromium.weblayer.demo/.WebLayerAnimationDemoActivity
```
