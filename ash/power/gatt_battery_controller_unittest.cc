// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_controller.h"

#include "ash/power/fake_gatt_battery_poller.h"
#include "ash/power/gatt_battery_poller.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;

namespace ash {

namespace {

class FakeGattBatteryPollerFactory : public GattBatteryPoller::Factory {
 public:
  FakeGattBatteryPollerFactory() = default;
  ~FakeGattBatteryPollerFactory() override = default;

  FakeGattBatteryPoller* GetFakeBatteryPoller(const std::string& address) {
    auto it = poller_map_.find(address);
    return it != poller_map_.end() ? it->second : nullptr;
  }

  int pollers_created_count() { return pollers_created_count_; }

 private:
  std::unique_ptr<GattBatteryPoller> BuildInstance(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      std::unique_ptr<base::OneShotTimer> poll_timer) override {
    ++pollers_created_count_;
    auto fake_gatt_battery_poller = std::make_unique<FakeGattBatteryPoller>(
        device_address,
        base::BindOnce(&FakeGattBatteryPollerFactory::OnPollerDestroyed,
                       base::Unretained(this), device_address));
    poller_map_[device_address] = fake_gatt_battery_poller.get();
    return std::move(fake_gatt_battery_poller);
  }

  void OnPollerDestroyed(const std::string address) {
    poller_map_.erase(address);
  }

  // Map of fake Pollers, indexed by the address of their Bluetooth device.
  base::flat_map<std::string, FakeGattBatteryPoller*> poller_map_;

  int pollers_created_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeGattBatteryPollerFactory);
};

}  // namespace

class GattBatteryControllerTest : public testing::Test {
 public:
  GattBatteryControllerTest() = default;
  ~GattBatteryControllerTest() override = default;

  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

    unpaired_mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            nullptr /* adapter */, 0 /* bluetooth_class */, "name_1",
            "address_1", false /* paired */, true /* connected */);
    paired_mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            nullptr /* adapter */, 0 /* bluetooth_class */, "name_2",
            "address_2", true /* paired */, true /* connected */);

    fake_gatt_battery_poller_factory_ =
        std::make_unique<FakeGattBatteryPollerFactory>();
    GattBatteryPoller::Factory::SetFactoryForTesting(
        fake_gatt_battery_poller_factory_.get());

    gatt_controller_ = std::make_unique<GattBatteryController>(mock_adapter_);
  }

  void TearDown() override {
    GattBatteryPoller::Factory::SetFactoryForTesting(nullptr);
  }

  void NotifyConnectedStateChanged(device::BluetoothDevice* device,
                                   bool is_connected) {
    DCHECK(gatt_controller_);
    gatt_controller_->DeviceConnectedStateChanged(mock_adapter_.get(), device,
                                                  is_connected);
  }

  void NotifyDevicePairedChanged(device::BluetoothDevice* device,
                                 bool is_paired) {
    DCHECK(gatt_controller_);
    gatt_controller_->DevicePairedChanged(mock_adapter_.get(), device,
                                          is_paired);
  }

  void NotifyDeviceAdded(device::BluetoothDevice* device) {
    DCHECK(gatt_controller_);
    gatt_controller_->DeviceAdded(mock_adapter_.get(), device);
  }

  void NotifyDeviceRemoved(device::BluetoothDevice* device) {
    DCHECK(gatt_controller_);
    gatt_controller_->DeviceRemoved(mock_adapter_.get(), device);
  }

  FakeGattBatteryPoller* GetFakeBatteryPoller(
      device::MockBluetoothDevice* mock_device) {
    return fake_gatt_battery_poller_factory_->GetFakeBatteryPoller(
        mock_device->GetAddress());
  }

  int pollers_created_count() {
    return fake_gatt_battery_poller_factory_->pollers_created_count();
  }

 protected:
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<device::MockBluetoothDevice> unpaired_mock_device_;
  std::unique_ptr<device::MockBluetoothDevice> paired_mock_device_;
  std::unique_ptr<FakeGattBatteryPollerFactory>
      fake_gatt_battery_poller_factory_;
  std::unique_ptr<GattBatteryController> gatt_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GattBatteryControllerTest);
};

TEST_F(GattBatteryControllerTest,
       CreatesPollerForNewDevices_ConnectedStateChanged) {
  // Indicate a connection to an unpaired device was made. A poller should not
  // be created.
  NotifyConnectedStateChanged(unpaired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_FALSE(GetFakeBatteryPoller(unpaired_mock_device_.get()));
  EXPECT_EQ(0, pollers_created_count());

  // A paired device became connected. Should create a poller.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       CreatesPollerForNewDevices_DevicePairedChanged) {
  // Indicate a device is now paired. A poller should be created.
  NotifyDevicePairedChanged(paired_mock_device_.get(), true /* is_paired */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());
}

TEST_F(GattBatteryControllerTest, CreatesPollerForNewDevices_DeviceAdded) {
  NotifyDeviceAdded(unpaired_mock_device_.get());
  EXPECT_FALSE(GetFakeBatteryPoller(unpaired_mock_device_.get()));
  EXPECT_EQ(0, pollers_created_count());

  NotifyDeviceAdded(paired_mock_device_.get());
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       DoesntCreatePollerForAlreadyConnectedDevices_ConnectedStateChanged) {
  // Simulate a device connected, should create a poller.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());

  // Calls to DeviceConnectedStateChanged() with a connected device doesn't
  // create new instances.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_EQ(1, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       DoesntCreatePollerForAlreadyConnectedDevices_DeviceAdded) {
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());

  // Calls to DeviceAdded() with a connected device doesn't create new
  // instances.
  NotifyDeviceAdded(paired_mock_device_.get());
  EXPECT_EQ(1, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       RemovesDisconnectedDevices_ConnectedStateChanged) {
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));

  // Indicate a device is no longer connected. Should stop tracking that device.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              false /* is_connected */);
  EXPECT_FALSE(GetFakeBatteryPoller(paired_mock_device_.get()));
}

TEST_F(GattBatteryControllerTest,
       RemovesDisconnectedDevices_DevicePairedChanged) {
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));

  // Indicate a device is no longer paired. Should stop tracking that device.
  NotifyDevicePairedChanged(paired_mock_device_.get(), false /* is_paired */);
  EXPECT_FALSE(GetFakeBatteryPoller(paired_mock_device_.get()));
}

TEST_F(GattBatteryControllerTest, RemovesDisconnectedDevices_DeviceRemoved) {
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));

  // Indicate a device is no longer present. Should stop tracking that device.
  NotifyDeviceRemoved(paired_mock_device_.get());
  EXPECT_FALSE(GetFakeBatteryPoller(paired_mock_device_.get()));
}

TEST_F(GattBatteryControllerTest,
       DoesntDoAnythingRemovingUntrackedDevices_ConnectedStateChanged) {
  // Indicate a device is no longer connected.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              false /* is_connected */);

  // Should not create any poller nor crash.
  EXPECT_EQ(0, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       DoesntDoAnythingRemovingUntrackedDevices_DeviceRemoved) {
  // Indicate a device is no longer connected.
  NotifyDeviceRemoved(paired_mock_device_.get());

  // Should not create any poller nor crash.
  EXPECT_EQ(0, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       CreatesNewPollerAfterDeviceReconnects_ConnectedStateChanged) {
  // Simulate a device connected.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());

  // Indicate the device is no longer connected.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              false /* is_connected */);
  EXPECT_FALSE(GetFakeBatteryPoller(paired_mock_device_.get()));

  // Reconnect the device.
  NotifyConnectedStateChanged(paired_mock_device_.get(),
                              true /* is_connected */);
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(2, pollers_created_count());
}

TEST_F(GattBatteryControllerTest,
       CreatesNewPollerAfterDeviceReconnects_DeviceAdded_DeviceRemoved) {
  NotifyDeviceAdded(paired_mock_device_.get());
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(1, pollers_created_count());

  // Indicate the device is no longer connected.
  NotifyDeviceRemoved(paired_mock_device_.get());
  EXPECT_FALSE(GetFakeBatteryPoller(paired_mock_device_.get()));

  // Reconnect the device.
  NotifyDeviceAdded(paired_mock_device_.get());
  EXPECT_TRUE(GetFakeBatteryPoller(paired_mock_device_.get()));
  EXPECT_EQ(2, pollers_created_count());
}

}  // namespace ash
