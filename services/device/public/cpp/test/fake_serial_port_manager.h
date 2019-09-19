// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

class FakeSerialPortManager : public mojom::SerialPortManager {
 public:
  FakeSerialPortManager();
  ~FakeSerialPortManager() override;

  void AddBinding(mojom::SerialPortManagerRequest request);
  void AddPort(mojom::SerialPortInfoPtr port);

  // mojom::SerialPortManager
  void GetDevices(GetDevicesCallback callback) override;
  void GetPort(
      const base::UnguessableToken& token,
      mojom::SerialPortRequest request,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher) override;

 private:
  std::map<base::UnguessableToken, mojom::SerialPortInfoPtr> ports_;
  mojo::BindingSet<mojom::SerialPortManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialPortManager);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_MANAGER_H_
