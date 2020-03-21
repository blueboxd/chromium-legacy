# How does [`WebChromeClient#onCreateWindow`](https://developer.android.com/reference/android/webkit/WebChromeClient#onCreateWindow(android.webkit.WebView,%20boolean,%20boolean,%20android.os.Message)) work?

[TOC]

## Summary

This is a technical explanation of how `onCreateWindow` and the related API are
implemented from content layer APIs.

## Example usage

Let's look at example code snippets first to see how an app could use these API:

On the app side (in Java):

```java
// Configure parent WebView.
WebView webView = ...;
webView.getSettings().setJavaScriptEnabled(true);
webView.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
webView.getSettings().setSupportMultipleWindows(true);

webView.setWebChromeClient(new WebChromeClient() {
    @Override
    public boolean onCreateWindow(
            WebView view, boolean isDialog, boolean isUserGesture, Message resultMsg) {
        // Create child WebView. It is better to not reuse an existing WebView.
        WebView childWebView = ...;

        WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
        transport.setWebView(childWebView);
        resultMsg.sentToTarget();
        return true;
    }
});

webView.loadUrl(...);
```

On the web page side (in JavaScript):

```javascript
window.open("www.example.com");
```

## What happened under the hood

1. When the parent WebView loads the web page and runs the JavaScript snippet,
   [`AwWebContentsDelegate::AddNewContents`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/browser/aw_web_contents_delegate.h;drc=a418951d0e0939a779baf00842b77806ba2c2fda;l=44)
   will be called. The corresponding Java side
   [`AwWebContentsDelegate#addNewContents`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/java/src/org/chromium/android_webview/AwWebContentsDelegate.java;drc=c3df19e6cd403bebb24b7418b441c861332fbfda;l=29)
   is called from the native.

1. At the same time,
   [`AwContents::SetPendingWebContentsForPopup`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/browser/aw_contents.cc;drc=f32bfd577cabaabfb08dfa06ce2317ac65cb8aab;l=1072)
   creates native popup AwContents with the given `WebContents` and stores it as
   `pending_contents_` in the parent `AwContents` object without Java
   counterpart created. Note that since `pending_contents_` can only store one
   popup AwContents, WebView doesn't support multiple pending popups.

1. `WebChromeClient#onCreateWindow` is called from step 1, with the code snippet
   above, `childWebView` is set to the `WebViewTransport` and
   `resultMsg.sendToTarget()` will send the `childWebView` to its receiver.

1. `WebViewContentsClientAdapter` has a handler that receives the message sent
   from `resultMsg.sendToTarget()`. It will trigger
   [`WebViewChromium#completeWindowCreation`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/glue/java/src/com/android/webview/chromium/WebViewChromium.java;drc=8988f749860f6299bab8d76a89e56134ccdf4bdb;l=268),
   then
   [`AwContents#supplyContentsForPopup`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/java/src/org/chromium/android_webview/AwContents.java;drc=83df35b1f2713897ff958dab849d103438f4457a;l=1300)
   is called on the parent WebView/AwContents.

1. `AwContents#supplyContentsForPopup` calls
   [`AwContents#receivePopupContents`](https://source.chromium.org/chromium/chromium/src/+/master:android_webview/java/src/org/chromium/android_webview/AwContents.java;drc=83df35b1f2713897ff958dab849d103438f4457a;l=1319)
   on the child WebView/AwContents. Child AwContents deletes the existing native
   AwContents from the child WebView/AwContents, and pairs it with the
   `pending_contents_` from the parent WebView/AwContents. In order to preserve
   the status of the child WebView, all the flags and configurations need to be
   re-applied to the `pending_contents_`. Loading on the native AwContents is
   also resumed.
