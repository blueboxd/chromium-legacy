// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/sandbox_status_service.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

namespace service_manager {

// static
void SandboxStatusService::MakeSelfOwnedReceiver(
    mojo::PendingReceiver<mojom::SandboxStatusService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SandboxStatusService>(),
                              std::move(receiver));
}

SandboxStatusService::SandboxStatusService() = default;

SandboxStatusService::~SandboxStatusService() = default;

void SandboxStatusService::GetSandboxStatus(GetSandboxStatusCallback callback) {
  std::move(callback).Run(
      service_manager::SandboxLinux::GetInstance()->GetStatus());
}

}  // namespace service_manager
