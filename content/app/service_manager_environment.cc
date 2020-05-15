// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/service_manager_environment.h"

#include <utility>

#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/startup_data_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace content {

ServiceManagerEnvironment::ServiceManagerEnvironment(
    std::unique_ptr<BrowserProcessSubThread> io_thread)
    : io_thread_(std::move(io_thread)) {
  scoped_refptr<base::SingleThreadTaskRunner> mojo_ipc_task_runner =
      io_thread_->task_runner();
  if (base::FeatureList::IsEnabled(features::kMojoDedicatedThread)) {
    mojo_ipc_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    mojo_ipc_task_runner = mojo_ipc_thread_.task_runner();
  }
  mojo_ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      mojo_ipc_task_runner, mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  service_manager_context_ =
      std::make_unique<ServiceManagerContext>(io_thread_->task_runner());
  ServiceManagerConnection::GetForProcess()->Start();
}

ServiceManagerEnvironment::~ServiceManagerEnvironment() = default;

std::unique_ptr<StartupDataImpl>
ServiceManagerEnvironment::CreateBrowserStartupData() {
  auto startup_data = std::make_unique<StartupDataImpl>();
  startup_data->io_thread = std::move(io_thread_);
  startup_data->mojo_ipc_support = std::move(mojo_ipc_support_);
  startup_data->service_manager_shutdown_closure =
      base::BindOnce(&ServiceManagerContext::ShutDown,
                     base::Unretained(service_manager_context_.get()));
  return startup_data;
}

}  // namespace content
