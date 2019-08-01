// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "fuchsia/runners/cast/cast_runner.h"

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePump::Type::IO);
  base::RunLoop run_loop;

  CastRunner runner(base::fuchsia::ServiceDirectory::GetDefault(),
                    WebContentRunner::CreateIncognitoWebContext());

  // Run until there are no Components, or the last service client channel is
  // closed.
  // TODO(https://crbug.com/952560): Implement Components v2 graceful exit.
  run_loop.Run();

  return 0;
}
