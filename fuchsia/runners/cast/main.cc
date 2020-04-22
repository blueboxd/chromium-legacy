// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/message_loop/message_pump_type.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "fuchsia/base/config_reader.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/runners/cast/cast_runner.h"

namespace {

bool IsHeadless() {
  constexpr char kHeadlessConfigKey[] = "headless";

  const base::Optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (config) {
    base::Optional<bool> headless = config->FindBoolPath(kHeadlessConfigKey);
    return headless && *headless;
  }

  return false;
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::RunLoop run_loop;

  base::CommandLine::Init(argc, argv);
  CHECK(cr_fuchsia::InitLoggingFromCommandLine(
      *base::CommandLine::ForCurrentProcess()))
      << "Failed to initialize logging.";

  cr_fuchsia::RegisterFuchsiaDirScheme();

  CastRunner runner(IsHeadless());
  base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner> binding(
      base::fuchsia::ComponentContextForCurrentProcess()->outgoing().get(),
      &runner);

  base::fuchsia::ComponentContextForCurrentProcess()
      ->outgoing()
      ->ServeFromStartupInfo();

  // Run until there are no Components, or the last service client channel is
  // closed.
  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}
