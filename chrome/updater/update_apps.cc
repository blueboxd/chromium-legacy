// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_apps.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_service.h"

namespace updater {

int UpdateApps() {
  // TODO(crbug.com/1048653): Try to connect to an existing OOP service. For
  // now, run an in-process service.

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop runloop;
  auto service =
      std::make_unique<UpdateService>(base::MakeRefCounted<Configurator>());
  service->UpdateAll(base::BindOnce(
      [](base::OnceClosure quit, update_client::Error error) {
        VLOG(0) << "UpdateAll complete: error = " << static_cast<int>(error);
        std::move(quit).Run();
      },
      runloop.QuitWhenIdleClosure()));
  runloop.Run();
  return 0;
}

}  // namespace updater
