// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_TEST_SHADER_H_
#define COMPONENTS_METAL_UTIL_TEST_SHADER_H_

#include "base/callback.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/metal_util/metal_util_export.h"

namespace metal {

enum class METAL_UTIL_EXPORT TestShaderResult {
  // Not attempted (e.g, because macOS version does not support Metal).
  kNotAttempted,
  // Shader compile succeeded.
  kSucceeded,
  // Shader compile failed.
  kFailed,
  // Shader compile timed out.
  kTimedOut,
};

using TestShaderCallback =
    base::OnceCallback<void(TestShaderResult result,
                            const base::TimeDelta& method_time,
                            const base::TimeDelta& compile_time)>;

// A default timeout value for compiling the test shader.
constexpr base::TimeDelta kTestShaderTimeout = base::TimeDelta::FromMinutes(1);

// Return the value kTestShaderTimeoutTime for |method_time| and |compile_time|
// if they time out.
constexpr base::TimeDelta kTestShaderTimeForever =
    base::TimeDelta::FromMinutes(3);

// A default delay before attempting to compile the test shader.
constexpr base::TimeDelta kTestShaderDelay = base::TimeDelta::FromSeconds(90);

// Attempt to asynchronously compile a trivial Metal shader. If |delay| is zero,
// then compile synchronously, otherwise, post a delayed task to do the compile.
// |callback| with the result when the shader succeeds or after |timeout| has
// elapsed.
//
// This is used to determine of the Metal shader compiler is resposive. Note
// that |callback| will be called either on another thread or inside the
// TestShader function call.
// https://crbug.com/974219
void METAL_UTIL_EXPORT
TestShader(TestShaderCallback callback,
           const base::TimeDelta& delay = kTestShaderDelay,
           const base::TimeDelta& timeout = kTestShaderTimeout);

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_TEST_SHADER_H_
