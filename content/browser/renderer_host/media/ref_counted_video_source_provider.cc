// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"

namespace content {

RefCountedVideoSourceProvider::RefCountedVideoSourceProvider(
    video_capture::mojom::VideoSourceProviderPtr source_provider,
    video_capture::mojom::DeviceFactoryProviderPtr device_factory_provider,
    base::OnceClosure destruction_cb)
    : source_provider_(std::move(source_provider)),
      device_factory_provider_(std::move(device_factory_provider)),
      destruction_cb_(std::move(destruction_cb)) {}

RefCountedVideoSourceProvider::~RefCountedVideoSourceProvider() {
  std::move(destruction_cb_).Run();
}

base::WeakPtr<RefCountedVideoSourceProvider>
RefCountedVideoSourceProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RefCountedVideoSourceProvider::ShutdownServiceAsap() {
  device_factory_provider_->ShutdownServiceAsap();
}

void RefCountedVideoSourceProvider::SetRetryCount(int32_t count) {
  device_factory_provider_->SetRetryCount(count);
}

void RefCountedVideoSourceProvider::ReleaseProviderForTesting() {
  source_provider_.reset();
}

}  // namespace content
