// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENT_RENDERER_OVERLAY_MANIFEST_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENT_RENDERER_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace android_webview {

// Returns the manifest Android WebView amends to Content's content_renderer
// service manifest. This allows WebView to extend the capabilities exposed
// and/or required by content_renderer service instances.
const service_manager::Manifest& GetAWContentRendererOverlayManifest();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENT_RENDERER_OVERLAY_MANIFEST_H_
