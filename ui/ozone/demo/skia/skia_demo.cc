// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/demo/skia/skia_renderer_factory.h"
#include "ui/ozone/demo/window_manager.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  base::debug::EnableInProcessStackDumping();

  // Initialize logging so we can enable VLOG messages.
  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  // Initialize tracing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  }

  mojo::core::Init();

  // Build UI thread task executor. This is used by platform
  // implementations for event polling & running background tasks.
  base::SingleThreadTaskExecutor main_task_executor(
      base::MessagePump::Type::UI);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("SkiaDemo");

  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  params.using_mojo = ui::OzonePlatform::EnsureInstance()
                          ->GetPlatformProperties()
                          .requires_mojo;
  ui::OzonePlatform::InitializeForUI(params);
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");

  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();

  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper;
  if (!params.using_mojo) {
    // OzoneGpuTestHelper transports Chrome IPC messages between host & gpu code
    // in single process mode. We don't use both Chrome IPC and mojo, so only
    // initialize it for non-mojo platforms.
    gpu_helper = std::make_unique<ui::OzoneGpuTestHelper>();
    gpu_helper->Initialize(base::ThreadTaskRunnerHandle::Get());
  }

  base::RunLoop run_loop;

  ui::WindowManager window_manager(std::make_unique<ui::SkiaRendererFactory>(),
                                   run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
