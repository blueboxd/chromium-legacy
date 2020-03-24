// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::Return;

namespace gpu {
namespace gles2 {

// Class to use to test that functions which need feature flags or
// extensions always return INVALID_OPERATION if the feature flags is not
// enabled or extension is not present.
class GLES2DecoderTestDisabledExtensions : public GLES2DecoderTest {
 public:
  GLES2DecoderTestDisabledExtensions() = default;
};
INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestDisabledExtensions,
                         ::testing::Bool());

class GLES2DecoderTestWithBlendEquationAdvanced : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithBlendEquationAdvanced() = default;
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 2.0";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "GL_KHR_blend_equation_advanced";
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestWithBlendEquationAdvanced,
                         ::testing::Bool());

class GLES2DecoderTestWithEXTMultisampleCompatibility
    : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithEXTMultisampleCompatibility() = default;

  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.1";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "GL_EXT_multisample_compatibility ";
    InitDecoder(init);
  }
};
INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestWithEXTMultisampleCompatibility,
                         ::testing::Bool());

class GLES2DecoderTestWithBlendFuncExtended : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithBlendFuncExtended() = default;
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "GL_EXT_blend_func_extended";
    InitDecoder(init);
  }
};
INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestWithBlendFuncExtended,
                         ::testing::Bool());

class GLES2DecoderTestWithCHROMIUMFramebufferMixedSamples
    : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithCHROMIUMFramebufferMixedSamples() = default;
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.1";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "GL_NV_framebuffer_mixed_samples ";
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestWithCHROMIUMFramebufferMixedSamples,
                         ::testing::Bool());

class GLES2DecoderTestWithCHROMIUMRasterTransport : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithCHROMIUMRasterTransport() = default;
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 2.0";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "chromium_raster_transport";
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTestWithCHROMIUMRasterTransport,
                         ::testing::Bool());

class GLES3DecoderTestWithEXTWindowRectangles : public GLES3DecoderTest {
 public:
  GLES3DecoderTestWithEXTWindowRectangles() = default;
  void SetUp() override {
    InitState init;
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    init.gl_version = "OpenGL ES 3.0";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    init.bind_generates_resource = true;
    init.extensions = "GL_EXT_window_rectangles";
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES3DecoderTestWithEXTWindowRectangles,
                         ::testing::Bool());

TEST_P(GLES3DecoderTestWithEXTWindowRectangles,
       WindowRectanglesEXTImmediateValidArgs) {
  cmds::WindowRectanglesEXTImmediate& cmd =
      *GetImmediateAs<cmds::WindowRectanglesEXTImmediate>();
  SpecializedSetup<cmds::WindowRectanglesEXTImmediate, 0>(true);
  GLint temp[4 * 2] = {};

  // The backbuffer is still bound, so the expected result is actually a reset
  // to the default state. (Window rectangles don't affect the backbuffer.)
  EXPECT_CALL(*gl_, WindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr));
  cmd.Init(GL_INCLUSIVE_EXT, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_extensions_autogen.h"

}  // namespace gles2
}  // namespace gpu
