// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/test_shader.h"

#import <Metal/Metal.h>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/metal_util/device.h"

namespace metal {

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
    "    return %f * in.color;\n"
    "}\n"
    "";

// State shared between the compiler callback and the caller.
class API_AVAILABLE(macos(10.11)) TestShaderState
    : public base::RefCountedThreadSafe<TestShaderState> {
 public:
  TestShaderState(TestShaderCallback callback, const base::TimeDelta& timeout)
      : callback_(std::move(callback)),
        timeout_(timeout),
        start_time_(base::TimeTicks::Now()) {}

  // Called when the completion callback happens.
  void OnCompletionHandlerCalled(TestShaderResult result) {
    {
      base::AutoLock lock(lock_);
      result_ = result;
      completion_handler_called_time_ = base::TimeTicks::Now();
    }
    RunCallbackIfReady();
  }

  // Called when the call to newLibraryWithSource completes. Note that this may
  // happen before or after OnCompletionHandlerCalled is called.
  void OnNewLibraryWithSourceCompleted() {
    {
      base::AutoLock lock(lock_);
      method_completed_time_ = base::TimeTicks::Now();
    }
    RunCallbackIfReady();
  }

  // Called on timeout. This is always called eventually.
  void OnTimeout() {
    {
      base::AutoLock lock(lock_);
      timed_out_ = true;
    }
    RunCallbackIfReady();
  }

 protected:
  virtual ~TestShaderState() = default;
  void RunCallbackIfReady() {
    base::OnceClosure closure;
    {
      base::AutoLock lock(lock_);
      // Early-out if we have already run the callback.
      if (!callback_)
        return;

      // We're ready to run the callback if we have timed out, or if both times
      // have been set.
      if (!timed_out_ && (method_completed_time_.is_null() ||
                          completion_handler_called_time_.is_null()))
        return;

      // It would be truly bizarre to have newLibraryWithSource hang, but yet
      // have the completion handler have been called. Crash if we see this,
      // rather than trying to reason about it.
      if (method_completed_time_.is_null())
        CHECK(completion_handler_called_time_.is_null());

      // We will use a single histogram for both "how long did compile take" and
      // "did compile complete in 1 minute" by pushing timeouts into the maximum
      // bucket of UMA_HISTOGRAM_MEDIUM_TIMES.
      base::TimeDelta method_time = kTestShaderTimeForever;
      if (!method_completed_time_.is_null())
        method_time = std::min(timeout_, method_completed_time_ - start_time_);
      base::TimeDelta compile_time = kTestShaderTimeForever;
      if (!completion_handler_called_time_.is_null())
        compile_time =
            std::min(timeout_, completion_handler_called_time_ - start_time_);

      // Set up the callback to execute once we release the lock.
      closure = base::BindOnce(std::move(callback_), result_, method_time,
                               compile_time);
    }
    if (closure)
      std::move(closure).Run();
  }

  base::Lock lock_;
  TestShaderCallback callback_;
  TestShaderResult result_ = TestShaderResult::kTimedOut;
  const base::TimeDelta timeout_;
  const base::TimeTicks start_time_;
  base::TimeTicks method_completed_time_;
  base::TimeTicks completion_handler_called_time_;
  bool timed_out_ = false;

  friend class base::RefCountedThreadSafe<TestShaderState>;
};

void TestShaderWithDeviceNow(base::scoped_nsprotocol<id<MTLDevice>> device,
                             const base::TimeDelta& timeout,
                             TestShaderCallback callback)
    API_AVAILABLE(macos(10.11)) {
  // Initialize the TestShaderState and post the timeout callback, before
  // calling into the Metal API.
  auto state =
      base::MakeRefCounted<TestShaderState>(std::move(callback), timeout);
  base::PostDelayedTask(FROM_HERE, {base::ThreadPool()},
                        base::BindOnce(&TestShaderState::OnTimeout, state),
                        timeout);

  const std::string shader_source =
      base::StringPrintf(kTestShaderSource, base::RandDouble());
  base::scoped_nsobject<NSString> source([[NSString alloc]
      initWithCString:shader_source.c_str()
             encoding:NSASCIIStringEncoding]);
  base::scoped_nsobject<MTLCompileOptions> options(
      [[MTLCompileOptions alloc] init]);
  MTLNewLibraryCompletionHandler completion_handler =
      ^(id<MTLLibrary> library, NSError* error) {
        if (library)
          state->OnCompletionHandlerCalled(TestShaderResult::kSucceeded);
        else
          state->OnCompletionHandlerCalled(TestShaderResult::kFailed);
      };
  [device newLibraryWithSource:source
                       options:options
             completionHandler:completion_handler];
  state->OnNewLibraryWithSourceCompleted();
}

}  // namespace

void TestShader(TestShaderCallback callback,
                const base::TimeDelta& delay,
                const base::TimeDelta& timeout) {
  if (@available(macOS 10.11, *)) {
    base::scoped_nsprotocol<id<MTLDevice>> device(CreateDefaultDevice());
    if (device) {
      if (delay.is_zero()) {
        TestShaderWithDeviceNow(device, timeout, std::move(callback));
      } else {
        base::PostDelayedTask(FROM_HERE, {base::ThreadPool()},
                              base::BindOnce(&TestShaderWithDeviceNow, device,
                                             timeout, std::move(callback)),
                              delay);
      }
      return;
    }
  }
  std::move(callback).Run(TestShaderResult::kNotAttempted, base::TimeDelta(),
                          base::TimeDelta());
}

}  // namespace metal
