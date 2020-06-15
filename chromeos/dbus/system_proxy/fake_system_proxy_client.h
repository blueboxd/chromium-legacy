// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_
#define CHROMEOS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_

#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeSystemProxyClient
    : public SystemProxyClient,
      public SystemProxyClient::TestInterface {
 public:
  FakeSystemProxyClient();
  FakeSystemProxyClient(const FakeSystemProxyClient&) = delete;
  FakeSystemProxyClient& operator=(const FakeSystemProxyClient&) = delete;
  ~FakeSystemProxyClient() override;

  // SystemProxyClient implementation.
  void SetAuthenticationDetails(
      const system_proxy::SetAuthenticationDetailsRequest& request,
      SetAuthenticationDetailsCallback callback) override;
  void ShutDownDaemon(ShutDownDaemonCallback callback) override;
  void ConnectToWorkerActiveSignal(WorkerActiveCallback callback) override;

  SystemProxyClient::TestInterface* GetTestInterface() override;

  // SystemProxyClient::TestInterface implementation.
  int GetSetAuthenticationDetailsCallCount() const override;
  int GetShutDownCallCount() const override;
  system_proxy::SetAuthenticationDetailsRequest
  GetLastAuthenticationDetailsRequest() const override;

 private:
  system_proxy::SetAuthenticationDetailsRequest last_set_auth_details_request_;
  int set_credentials_call_count_ = 0;
  int shut_down_call_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_
