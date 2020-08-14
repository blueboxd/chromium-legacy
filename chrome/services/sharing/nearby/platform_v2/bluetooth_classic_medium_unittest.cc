// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/bluetooth_classic_medium.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {
const char kDeviceAddress1[] = "DeviceAddress1";
const char kDeviceAddress2[] = "DeviceAddress2";
const char kDeviceName1[] = "DeviceName1";
const char kDeviceName2[] = "DeviceName2";
}  // namespace

class BluetoothClassicMediumTest : public testing::Test {
 public:
  BluetoothClassicMediumTest() = default;
  ~BluetoothClassicMediumTest() override = default;
  BluetoothClassicMediumTest(const BluetoothClassicMediumTest&) = delete;
  BluetoothClassicMediumTest& operator=(const BluetoothClassicMediumTest&) =
      delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();

    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    bluetooth_classic_medium_ =
        std::make_unique<BluetoothClassicMedium>(remote_adapter_.get());

    discovery_callback_ = {
        .device_discovered_cb =
            [this](api::BluetoothDevice& device) {
              last_device_discovered_ = &device;
              std::move(on_device_discovered_callback_).Run();
            },
        .device_name_changed_cb =
            [this](api::BluetoothDevice& device) {
              last_device_name_changed_ = &device;
              std::move(on_device_name_changed_callback_).Run();
            },
        .device_lost_cb =
            [this](api::BluetoothDevice& device) {
              DCHECK_EQ(expected_last_device_lost_, &device);
              std::move(on_device_lost_callback_).Run();
            }};
  }

 protected:
  void StartDiscovery() {
    EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
    EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(discovery_callback_));
    EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());
  }

  void StopDiscovery() {
    base::RunLoop run_loop;
    fake_adapter_->SetDiscoverySessionDestroyedCallback(run_loop.QuitClosure());
    EXPECT_TRUE(bluetooth_classic_medium_->StopDiscovery());
    run_loop.Run();

    EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  }

  void NotifyDeviceAdded(const std::string& address, const std::string& name) {
    base::RunLoop run_loop;
    on_device_discovered_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceAdded(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  void NotifyDeviceChanged(const std::string& address,
                           const std::string& name) {
    base::RunLoop run_loop;
    on_device_name_changed_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceChanged(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  void NotifyDeviceRemoved(const std::string& address,
                           const std::string& name) {
    base::RunLoop run_loop;
    on_device_lost_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceRemoved(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  bluetooth::FakeAdapter* fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BluetoothClassicMedium> bluetooth_classic_medium_;
  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;

  api::BluetoothDevice* last_device_discovered_ = nullptr;
  api::BluetoothDevice* last_device_name_changed_ = nullptr;
  api::BluetoothDevice* expected_last_device_lost_ = nullptr;

  base::OnceClosure on_device_discovered_callback_;
  base::OnceClosure on_device_name_changed_callback_;
  base::OnceClosure on_device_lost_callback_;

 private:
  bluetooth::mojom::DeviceInfoPtr CreateDeviceInfo(const std::string& address,
                                                   const std::string& name) {
    auto device_info = bluetooth::mojom::DeviceInfo::New();
    device_info->address = address;
    device_info->name = name;
    device_info->name_for_display = name;
    return device_info;
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothClassicMediumTest, TestDiscovery_StartDiscoveryIsIdempotent) {
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(discovery_callback_));
  EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());

  EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(discovery_callback_));
  EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_StartDiscoveryError) {
  fake_adapter_->SetShouldDiscoverySucceed(false);
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  EXPECT_FALSE(bluetooth_classic_medium_->StartDiscovery(discovery_callback_));
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_DeviceDiscovered) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());
  auto* first_device_discovered = last_device_discovered_;

  NotifyDeviceAdded(kDeviceAddress2, kDeviceName2);
  EXPECT_EQ(kDeviceName2, last_device_discovered_->GetName());

  EXPECT_NE(first_device_discovered, last_device_discovered_);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_DeviceNameChanged) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());

  NotifyDeviceChanged(kDeviceAddress1, kDeviceName2);
  EXPECT_EQ(kDeviceName2, last_device_name_changed_->GetName());

  EXPECT_EQ(last_device_name_changed_, last_device_discovered_);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_DeviceLost) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());

  expected_last_device_lost_ = last_device_discovered_;
  NotifyDeviceRemoved(kDeviceAddress1, kDeviceName1);

  StopDiscovery();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
