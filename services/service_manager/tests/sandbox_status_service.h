// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_TESTS_SANDBOX_STATUS_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_TESTS_SANDBOX_STATUS_SERVICE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/tests/sandbox_status.test-mojom.h"

namespace service_manager {

class SandboxStatusService : public mojom::SandboxStatusService {
 public:
  static void MakeSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::SandboxStatusService> receiver);

  SandboxStatusService();
  ~SandboxStatusService() override;

 private:
  // mojom::SandboxStatusService:
  void GetSandboxStatus(GetSandboxStatusCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(SandboxStatusService);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_TESTS_SANDBOX_STATUS_SERVICE_H_
