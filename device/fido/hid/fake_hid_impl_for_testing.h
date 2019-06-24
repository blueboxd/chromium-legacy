// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_
#define DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "device/fido/fido_constants.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace service_manager {
class Connector;
}

namespace device {

class MockFidoHidConnection : public device::mojom::HidConnection {
 public:
  explicit MockFidoHidConnection(device::mojom::HidDeviceInfoPtr device,
                                 device::mojom::HidConnectionRequest request,
                                 std::vector<uint8_t> connection_channel_id);

  ~MockFidoHidConnection() override;
  MOCK_METHOD1(ReadPtr, void(ReadCallback* callback));
  MOCK_METHOD3(WritePtr,
               void(uint8_t report_id,
                    const std::vector<uint8_t>& buffer,
                    WriteCallback* callback));

  void Read(ReadCallback callback) override;

  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;

  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;
  void SetNonce(base::span<uint8_t const> nonce);

  void ExpectWriteHidInit();
  void ExpectHidWriteWithCommand(FidoHidDeviceCommand cmd);

  const std::vector<uint8_t>& connection_channel_id() const {
    return connection_channel_id_;
  }
  const std::vector<uint8_t>& nonce() const { return nonce_; }

 private:
  mojo::Binding<device::mojom::HidConnection> binding_;
  device::mojom::HidDeviceInfoPtr device_;
  std::vector<uint8_t> nonce_;
  std::vector<uint8_t> connection_channel_id_;

  DISALLOW_COPY_AND_ASSIGN(MockFidoHidConnection);
};

class FakeFidoHidConnection : public device::mojom::HidConnection {
 public:
  explicit FakeFidoHidConnection(device::mojom::HidDeviceInfoPtr device);

  ~FakeFidoHidConnection() override;

  // device::mojom::HidConnection implemenation:
  void Read(ReadCallback callback) override;
  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;
  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;

  static bool mock_connection_error_;

 private:
  device::mojom::HidDeviceInfoPtr device_;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoHidConnection);
};

class FakeFidoHidManager : public device::mojom::HidManager {
 public:
  FakeFidoHidManager();
  ~FakeFidoHidManager() override;

  // Invoke AddDevice with a device info struct that mirrors a FIDO USB device.
  void AddFidoHidDevice(std::string guid);

  // device::mojom::HidManager implementation:
  void GetDevicesAndSetClient(
      device::mojom::HidManagerClientAssociatedPtrInfo client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void Connect(const std::string& device_guid,
               mojom::HidConnectionClientPtr connection_client,
               ConnectCallback callback) override;
  void AddBinding(mojo::ScopedMessagePipeHandle handle);
  void AddBinding2(device::mojom::HidManagerRequest request);
  void AddDevice(device::mojom::HidDeviceInfoPtr device);
  void AddDeviceAndSetConnection(device::mojom::HidDeviceInfoPtr device,
                                 device::mojom::HidConnectionPtr connection);
  void RemoveDevice(const std::string device_guid);

 private:
  std::map<std::string, device::mojom::HidDeviceInfoPtr> devices_;
  std::map<std::string, device::mojom::HidConnectionPtr> connections_;
  mojo::AssociatedInterfacePtrSet<device::mojom::HidManagerClient> clients_;
  mojo::BindingSet<device::mojom::HidManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoHidManager);
};

// ScopedFakeFidoHidManager automatically binds itself to the device service for
// the duration of its lifetime.
class ScopedFakeFidoHidManager : public FakeFidoHidManager {
 public:
  ScopedFakeFidoHidManager();
  ~ScopedFakeFidoHidManager() override;

  service_manager::Connector* service_manager_connector() {
    return connector_.get();
  }

 private:
  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeFidoHidManager);
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FAKE_HID_IMPL_FOR_TESTING_H_
