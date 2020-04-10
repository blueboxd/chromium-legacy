# CORS and WebView API

## What is CORS?

[Cross-Origin Resource Sharing (CORS)](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
is a well-established security feature to protect data from unexpected
cross-origin accesses.

## Purpose of this document
WebView provides some APIs that change the CORS behaviors, but details are not
explained in the API documents. This document aims to clarify such detailed
behaviors and implementation details to give WebView and chromium developers
hints to keep consistent behaviors among making code changes.

## TL;DR for new developers
If you are working on new WebView applications and finding a way to load local
resources, we recommend that you use [WebViewAssetLoader](https://developer.android.com/reference/androidx/webkit/WebViewAssetLoader)
as using other Android specific schemes is not covered by the open web platform
standards, and behavior details for new web features might be undefined or
changed. Using the WebViewAssetLoader API to load local resource on virtual
http/https URLs avoids these compatibility issues and allows the standard web
security model to apply.

## Android or WebView specific features

### intent:// URLs
intent:// URLs are used to send an [Android Intent](https://developer.android.com/guide/components/intents-filters.html)
via a web link. A site can provide an intent:// link for users so that users can
launch an Android application through the link.
See [Android Intents with Chrome](https://developer.chrome.com/multidevice/android/intents)
for details.

This is allowed only for the top-level navigation. If the site has a link to
an intent:// URL for an iframe, such frame navigation will be just blocked.

Also, the page can not use such intent:// URLs for sub-resources. For instance,
image loading for intent:// URLs and making requests via XMLHttpRequest or Fetch
API just fail. JavaScript APIs will fail with an error (ex. error callback,
rejected promise, etc).

### content:// URLs
content:// URLs are used to access resources provided via
[Android Content Providers](https://developer.android.com/guide/topics/providers/content-providers).
The access should be permitted via
[setAllowContentAccess](https://developer.android.com/reference/android/webkit/WebSettings#setAllowContentAccess(boolean))
API beforehand.
content:// pages can contain iframes that load content:// pages, but the parent
frame can not access into the iframe contents. Also only content:// pages can
specify content:// URLs for sub-resources.
Pages loaded with any scheme other than content:// can't load content:// page in
iframes and they can not specify content:// URLs for sub-resources.

### file:///android\_{asset,res}/ URLs
TODO

## WebView APIs

### setAllowFileAccessFromFileURLs
TODO

### setAllowUniversalAccessFromFileURLs
TODO

### shouldInterceptRequest
Custom scheme should not be permitted for CORS-enabled requests usually.
However, when shouldInterceptRequest is used, the API allows developers to
handle CORS-enabled requests over custom schemes.

When a custom scheme is used, `*` or `null` should appear in the
Access-Control-Allow-Origin response header as such custom scheme is processed
as an [opaque origin](https://html.spec.whatwg.org/multipage/origin.html#concept-origin-opaque).
