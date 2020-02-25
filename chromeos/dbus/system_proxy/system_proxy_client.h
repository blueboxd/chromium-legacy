// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_
#define CHROMEOS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// SystemProxyClient is used to communicate with the org.chromium.SystemProxy
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SYSTEM_PROXY) SystemProxyClient {
 public:
  using SetSystemTrafficCredentialsCallback = base::OnceCallback<void(
      const system_proxy::SetSystemTrafficCredentialsResponse& response)>;
  using ShutDownDaemonCallback =
      base::OnceCallback<void(const system_proxy::ShutDownResponse& response)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Returns how many times |SetSystemTrafficCredentials| was called.
    virtual int GetSetSystemTrafficCredentialsCallCount() const = 0;
    // Returns how many times |ShutDownDaemon| was called.
    virtual int GetShutDownCallCount() const = 0;

   protected:
    virtual ~TestInterface() {}
  };

  SystemProxyClient(const SystemProxyClient&) = delete;
  SystemProxyClient& operator=(const SystemProxyClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static SystemProxyClient* Get();

  // SystemProxy daemon D-Bus method calls. See org.chromium.SystemProxy.xml and
  // system_proxy_service.proto in Chromium OS code for the documentation of the
  // methods and request/response messages.
  virtual void SetSystemTrafficCredentials(
      const system_proxy::SetSystemTrafficCredentialsRequest& request,
      SetSystemTrafficCredentialsCallback callback) = 0;

  // When receiving a shut-down call, System-proxy will schedule a shut-down
  // task and reply. |callback| is called when the daemon starts to shut-down.
  virtual void ShutDownDaemon(ShutDownDaemonCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  SystemProxyClient();
  virtual ~SystemProxyClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SYSTEM_PROXY_SYSTEM_PROXY_CLIENT_H_
