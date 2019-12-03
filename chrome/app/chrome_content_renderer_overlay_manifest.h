// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CONTENT_RENDERER_OVERLAY_MANIFEST_H_
#define CHROME_APP_CHROME_CONTENT_RENDERER_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

// Returns the Manifest Chrome amends to Content's content_renderer service
// manifest. This allows Chrome to extend the set of capabilities exposed and/or
// required by content_renderer service instances.
const service_manager::Manifest& GetChromeContentRendererOverlayManifest();

#endif  // CHROME_APP_CHROME_CONTENT_RENDERER_OVERLAY_MANIFEST_H_
