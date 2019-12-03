// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_GPU_MANIFEST_H_
#define CONTENT_PUBLIC_APP_CONTENT_GPU_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace content {

// Returns the service manifest for the "content_gpu" service. Every GPU process
// is an instance of this service, so this manifest determines what capabilities
// are directly exposed and required by GPU processes through the Service
// Manager.
const service_manager::Manifest& GetContentGpuManifest();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_GPU_MANIFEST_H_
