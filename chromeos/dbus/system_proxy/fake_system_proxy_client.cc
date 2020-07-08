// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/system_proxy/fake_system_proxy_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"

namespace chromeos {

FakeSystemProxyClient::FakeSystemProxyClient() = default;

FakeSystemProxyClient::~FakeSystemProxyClient() = default;

void FakeSystemProxyClient::SetAuthenticationDetails(
    const system_proxy::SetAuthenticationDetailsRequest& request,
    SetAuthenticationDetailsCallback callback) {
  ++set_credentials_call_count_;
  last_set_auth_details_request_ = request;
  system_proxy::SetAuthenticationDetailsResponse response;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::ShutDownDaemon(ShutDownDaemonCallback callback) {
  ++shut_down_call_count_;
  system_proxy::ShutDownResponse response;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeSystemProxyClient::SetWorkerActiveSignalCallback(
    WorkerActiveCallback callback) {
  worker_active_callback_ = callback;
}
void FakeSystemProxyClient::SetAuthenticationRequiredSignalCallback(
    AuthenticationRequiredCallback callback) {
  auth_required_callback_ = callback;
}

void FakeSystemProxyClient::ConnectToWorkerSignals() {
  connect_to_worker_signals_called_ = true;
}

SystemProxyClient::TestInterface* FakeSystemProxyClient::GetTestInterface() {
  return this;
}

int FakeSystemProxyClient::GetSetAuthenticationDetailsCallCount() const {
  return set_credentials_call_count_;
}

int FakeSystemProxyClient::GetShutDownCallCount() const {
  return shut_down_call_count_;
}

system_proxy::SetAuthenticationDetailsRequest
FakeSystemProxyClient::GetLastAuthenticationDetailsRequest() const {
  return last_set_auth_details_request_;
}

void FakeSystemProxyClient::SendAuthenticationRequiredSignal(
    const system_proxy::AuthenticationRequiredDetails& details) {
  if (!connect_to_worker_signals_called_) {
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(auth_required_callback_, details));
}

}  // namespace chromeos
