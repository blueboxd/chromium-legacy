// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_test_environment.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "media/gpu/buildflags.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace media {
namespace test {

VideoTestEnvironment::VideoTestEnvironment() = default;
VideoTestEnvironment::~VideoTestEnvironment() = default;

void VideoTestEnvironment::SetUp() {
  // If using '--gtest_repeat' Setup/TearDown will be called multiple times on
  // the same environment, however the setup here should only be performed once.
  if (initialized_)
    return;

  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  LOG_ASSERT(logging::InitLogging(settings));

  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for video
  // tests to function correctly.
  TestTimeouts::Initialize();
  task_environment_ = std::make_unique<base::test::ScopedTaskEnvironment>(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  // Perform all static initialization that is required when running video
  // decoders in a test environment.
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

#if defined(USE_OZONE)
  // Initialize Ozone. This is necessary to gain access to the GPU for hardware
  // video decode acceleration.
  LOG(WARNING) << "Initializing Ozone Platform...\n"
                  "If this hangs indefinitely please call 'stop ui' first!";
  ui::OzonePlatform::InitParams params = {.single_process = false};
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();

  // Initialize the Ozone GPU helper. If this is not done an error will occur:
  // "Check failed: drm. No devices available for buffer allocation."
  // Note: If a task environment is not set up initialization will hang
  // indefinitely here.
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif

  initialized_ = true;
}

void VideoTestEnvironment::TearDown() {
}

base::FilePath::StringType VideoTestEnvironment::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
#if defined(OS_WIN)
  // On Windows the default file path string type is UTF16. Since the test name
  // is always returned in UTF8 we need to do a conversion here.
  return base::UTF8ToUTF16(test_info->name());
#else
  return test_info->name();
#endif
}

}  // namespace test
}  // namespace media
