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
  void SetSystemTrafficCredentials(
      const system_proxy::SetSystemTrafficCredentialsRequest& request,
      SetSystemTrafficCredentialsCallback callback) override;
  void ShutDownDaemon(ShutDownDaemonCallback callback) override;
  SystemProxyClient::TestInterface* GetTestInterface() override;

  // SystemProxyClient::TestInterface implementation.
  int GetSetSystemTrafficCredentialsCallCount() const override;
  int GetShutDownCallCount() const override;

 private:
  int set_credentials_call_count_ = 0;
  int shut_down_call_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SYSTEM_PROXY_FAKE_SYSTEM_PROXY_CLIENT_H_
