// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

int RunHelper(base::TestSuite* testSuite) {
  base::FeatureList::InitializeInstance(std::string(), std::string());
#if defined(USE_OZONE)
  base::SingleThreadTaskExecutor executor(base::MessagePump::Type::UI);
#else
  base::SingleThreadTaskExecutor executor(base::MessagePump::Type::IO);
#endif
  gpu::GLTestHelper::InitializeGLDefault();

  ::gles2::Initialize();
  return testSuite->Run();
}

}  // namespace

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  base::CommandLine::Init(argc, argv);

  mojo::core::Init();

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  testing::InitGoogleMock(&argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
}
