// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#import <Metal/Metal.h>

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "components/metal_util/test_shader.h"
#include "components/viz/common/gpu/metal_api_proxy.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace viz {

namespace {

struct API_AVAILABLE(macos(10.11)) MetalContextProviderImpl
    : public MetalContextProvider {
 public:
  explicit MetalContextProviderImpl(id<MTLDevice> device) {
    device_.reset([[MTLDeviceProxy alloc] initWithDevice:device]);
    command_queue_.reset([device_ newCommandQueue]);
    gr_context_ = GrContext::MakeMetal(device_, command_queue_);
    DCHECK(gr_context_);
  }
  ~MetalContextProviderImpl() override {
    // Because there are no guarantees that |device_| will not outlive |this|,
    // un-set the progress reporter on |device_|.
    [device_ setProgressReporter:nullptr];
  }
  void SetProgressReporter(gl::ProgressReporter* progress_reporter) override {
    [device_ setProgressReporter:progress_reporter];
  }
  GrContext* GetGrContext() override { return gr_context_.get(); }
  metal::MTLDevicePtr GetMTLDevice() override { return device_.get(); }

 private:
  base::scoped_nsobject<MTLDeviceProxy> device_;
  base::scoped_nsprotocol<id<MTLCommandQueue>> command_queue_;
  sk_sp<GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(MetalContextProviderImpl);
};

}  // namespace

// static
std::unique_ptr<MetalContextProvider> MetalContextProvider::Create() {
  if (@available(macOS 10.11, *)) {
    // First attempt to find a low power device to use.
    base::scoped_nsprotocol<id<MTLDevice>> device_to_use(
        metal::CreateDefaultDevice());
    if (!device_to_use) {
      DLOG(ERROR) << "Failed to find MTLDevice.";
      return nullptr;
    }
    return std::make_unique<MetalContextProviderImpl>(device_to_use.get());
  }
  // If no device was found, or if the macOS version is too old for Metal,
  // return no context provider.
  return nullptr;
}

}  // namespace viz
