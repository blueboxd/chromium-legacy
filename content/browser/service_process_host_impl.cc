// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_process_host_impl.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace content {

class ServiceProcessHostImpl::IOThreadState {
 public:
  IOThreadState(ServiceProcessHostImpl::Options options,
                const std::string& service_interface_name,
                mojo::PendingReceiver<mojom::ServiceControl> receiver) {
    UtilityProcessHost* host = new UtilityProcessHost();
    host->SetName(!options.display_name.empty()
                      ? options.display_name
                      : base::UTF8ToUTF16(service_interface_name));
    host->SetMetricsName(service_interface_name);
    host->SetSandboxType(options.sandbox_type);
    host->Start();
    host->BindInterface(mojom::ServiceControl::Name_, receiver.PassPipe());
    utility_process_host_ = host->AsWeakPtr();
  }

  ~IOThreadState() = default;

 private:
  base::WeakPtr<UtilityProcessHost> utility_process_host_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadState);
};

ServiceProcessHostImpl::ServiceProcessHostImpl(
    base::StringPiece service_interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe,
    Options options)
    : io_thread_state_(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
          std::move(options),
          service_interface_name.as_string(),
          remote_control_.BindNewPipeAndPassReceiver()) {
  remote_control_->BindServiceInterface(mojo::GenericPendingReceiver(
      service_interface_name, std::move(receiving_pipe)));
}

ServiceProcessHostImpl::~ServiceProcessHostImpl() = default;

// static
std::unique_ptr<ServiceProcessHost> ServiceProcessHost::Launch(
    base::StringPiece service_interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe,
    Options options) {
  return std::make_unique<ServiceProcessHostImpl>(
      service_interface_name, std::move(receiving_pipe), std::move(options));
}

}  // namespace content
