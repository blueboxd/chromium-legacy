// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/system_proxy/fake_system_proxy_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"

namespace chromeos {

FakeSystemProxyClient::FakeSystemProxyClient() = default;

FakeSystemProxyClient::~FakeSystemProxyClient() = default;

void FakeSystemProxyClient::SetSystemTrafficCredentials(
    const system_proxy::SetSystemTrafficCredentialsRequest& request,
    SetSystemTrafficCredentialsCallback callback) {
  ++set_credentials_call_count_;
  system_proxy::SetSystemTrafficCredentialsResponse response;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::ShutDownDaemon(ShutDownDaemonCallback callback) {
  ++shut_down_call_count_;
  system_proxy::ShutDownResponse response;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

SystemProxyClient::TestInterface* FakeSystemProxyClient::GetTestInterface() {
  return this;
}

int FakeSystemProxyClient::GetSetSystemTrafficCredentialsCallCount() const {
  return set_credentials_call_count_;
}

int FakeSystemProxyClient::GetShutDownCallCount() const {
  return shut_down_call_count_;
}

}  // namespace chromeos
