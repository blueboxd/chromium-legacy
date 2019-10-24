// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "components/viz/common/gpu/metal_api_proxy.h"
#include "third_party/skia/include/gpu/GrContext.h"

#import <Metal/Metal.h>

namespace viz {

namespace {

const char* kTestShaderSource =
    ""
    "#include <metal_stdlib>\n"
    "#include <simd/simd.h>\n"
    "typedef struct {\n"
    "    float4 clipSpacePosition [[position]];\n"
    "    float4 color;\n"
    "} RasterizerData;\n"
    "\n"
    "vertex RasterizerData vertexShader(\n"
    "    uint vertexID [[vertex_id]],\n"
    "    constant vector_float2 *positions[[buffer(0)]],\n"
    "    constant vector_float4 *colors[[buffer(1)]]) {\n"
    "  RasterizerData out;\n"
    "  out.clipSpacePosition = vector_float4(0.0, 0.0, 0.0, 1.0);\n"
    "  out.clipSpacePosition.xy = positions[vertexID].xy;\n"
    "  out.color = colors[vertexID];\n"
    "  return out;\n"
    "}\n"
    "\n"
    "fragment float4 fragmentShader(RasterizerData in [[stage_in]]) {\n"
    "    return in.color;\n"
    "}\n"
    "";

// The timeout for compiling the test shader. Set to be much longer than any
// shader compile should take, but still less than the watchdog timeout.
constexpr base::TimeDelta kTestShaderCompileTimeout =
    base::TimeDelta::FromSeconds(6);

// State shared between the compiler callback and the caller.
class API_AVAILABLE(macos(10.11)) TestShaderState
    : public base::RefCountedThreadSafe<TestShaderState> {
 public:
  TestShaderState() {}
  base::WaitableEvent event;

 protected:
  friend class base::RefCountedThreadSafe<TestShaderState>;
  virtual ~TestShaderState() {}
};

// Attempt to asynchronously compile a test shader. If the compile doesn't
// complete within a timeout, then return false.
bool CompileTestShader(id<MTLDevice> device) API_AVAILABLE(macos(10.11)) {
  auto state = base::MakeRefCounted<TestShaderState>();

  base::scoped_nsobject<NSString> source([[NSString alloc]
      initWithCString:kTestShaderSource
             encoding:NSASCIIStringEncoding]);
  base::scoped_nsobject<MTLCompileOptions> options(
      [[MTLCompileOptions alloc] init]);
  MTLNewLibraryCompletionHandler completion_handler =
      ^(id<MTLLibrary> library, NSError* error) {
        DCHECK(library);
        state->event.Signal();
      };
  [device newLibraryWithSource:source
                       options:options
             completionHandler:completion_handler];

  return state->event.TimedWait(kTestShaderCompileTimeout);
}

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
    // Compile a test shader, to see if the MTLCompilerService is responding
    // or not. If the compile times out, then don't try to use Metal.
    // https://crbug.com/974219
    bool did_compile = CompileTestShader(device_to_use);
    UMA_HISTOGRAM_BOOLEAN("Gpu.Metal.TestShaderCompileSucceeded", did_compile);
    if (!did_compile) {
      DLOG(ERROR) << "Compiling test MTLLibrary timed out.";
      return nullptr;
    }
    return std::make_unique<MetalContextProviderImpl>(device_to_use.get());
  }
  // If no device was found, or if the macOS version is too old for Metal,
  // return no context provider.
  return nullptr;
}

}  // namespace viz
