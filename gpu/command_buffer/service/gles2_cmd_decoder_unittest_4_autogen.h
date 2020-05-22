// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_4.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_

TEST_P(GLES2DecoderTest4, BlendEquationiOESValidArgs) {
  EXPECT_CALL(*gl_, BlendEquationiOES(1, GL_FUNC_SUBTRACT));
  SpecializedSetup<cmds::BlendEquationiOES, 0>(true);
  cmds::BlendEquationiOES cmd;
  cmd.Init(1, GL_FUNC_SUBTRACT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, BlendEquationSeparateiOESValidArgs) {
  EXPECT_CALL(*gl_,
              BlendEquationSeparateiOES(1, GL_FUNC_SUBTRACT, GL_FUNC_SUBTRACT));
  SpecializedSetup<cmds::BlendEquationSeparateiOES, 0>(true);
  cmds::BlendEquationSeparateiOES cmd;
  cmd.Init(1, GL_FUNC_SUBTRACT, GL_FUNC_SUBTRACT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, BlendFunciOESValidArgs) {
  EXPECT_CALL(*gl_, BlendFunciOES(1, 2, 3));
  SpecializedSetup<cmds::BlendFunciOES, 0>(true);
  cmds::BlendFunciOES cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, BlendFuncSeparateiOESValidArgs) {
  EXPECT_CALL(*gl_, BlendFuncSeparateiOES(1, 2, 3, 4, 5));
  SpecializedSetup<cmds::BlendFuncSeparateiOES, 0>(true);
  cmds::BlendFuncSeparateiOES cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, ColorMaskiOESValidArgs) {
  EXPECT_CALL(*gl_, ColorMaskiOES(1, true, true, true, true));
  SpecializedSetup<cmds::ColorMaskiOES, 0>(true);
  cmds::ColorMaskiOES cmd;
  cmd.Init(1, true, true, true, true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, IsEnablediOESValidArgs) {
  SpecializedSetup<cmds::IsEnablediOES, 0>(true);
  cmds::IsEnablediOES cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, IsEnablediOESInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsEnablediOES, 0>(false);
  cmds::IsEnablediOES cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_
