// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/gl_test_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace vr {

// Failing on Win7 Tests (dbg)(1). crbug.com/1035767
#if defined(OS_WIN)
#define MAYBE_InitializeAndCleanup DISABLED_InitializeAndCleanup
#else
#define MAYBE_InitializeAndCleanup InitializeAndCleanup
#endif

TEST(GlTestEnvironmentTest, MAYBE_InitializeAndCleanup) {
  GlTestEnvironment gl_test_environment(gfx::Size(100, 100));
  EXPECT_NE(gl_test_environment.GetFrameBufferForTesting(), 0u);
  EXPECT_EQ(glGetError(), (GLenum)GL_NO_ERROR);
  // We just test that clean up doesn't crash.
}

}  // namespace vr
