// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_V8_SNAPSHOT_OVERLAY_MANIFEST_H_
#define CONTENT_PUBLIC_APP_V8_SNAPSHOT_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace content {

// Returns a service manifest overlay that should be amended to the manifest of
// any service which initializes V8. This gives the service access to preloaded
// snapshot files on startup.
const service_manager::Manifest& GetV8SnapshotOverlayManifest();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_V8_SNAPSHOT_OVERLAY_MANIFEST_H_
