// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};
}  // namespace

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdvertisement::ServiceData& service_data)
      : service_uuids(service_uuids), service_data(service_data) {}

  device::BluetoothAdvertisement::UUIDList service_uuids;
  device::BluetoothAdvertisement::ServiceData service_data;
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(RegisterAdvertisementArgs*));

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback) override {
    RegisterAdvertisementWithArgsStruct(
        new RegisterAdvertisementArgs(*advertisement_data->service_uuids(),
                                      *advertisement_data->service_data()));
  }

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override = default;
};

class NearbySharingFastInitiationManagerTest : public testing::Test {
 public:
  NearbySharingFastInitiationManagerTest(
      const NearbySharingFastInitiationManagerTest&) = delete;
  NearbySharingFastInitiationManagerTest& operator=(
      const NearbySharingFastInitiationManagerTest&) = delete;

 protected:
  NearbySharingFastInitiationManagerTest() = default;

  void SetUp() override {
    mock_adapter_ = base::MakeRefCounted<
        NiceMock<MockBluetoothAdapterWithAdvertisements>>();
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(Invoke(this, &NearbySharingFastInitiationManagerTest::
                                        OnAdapterRegisterAdvertisement));

    fast_initiation_manager_ =
        std::make_unique<FastInitiationManager>(mock_adapter_);
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_ = base::WrapUnique(args);
  }

  uint8_t GenerateFastInitV1Metadata() { return 0x00; }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;
  std::unique_ptr<FastInitiationManager> fast_initiation_manager_;

  std::unique_ptr<RegisterAdvertisementArgs> register_args_;
};

TEST_F(NearbySharingFastInitiationManagerTest, TestStartAdvertising) {
  fast_initiation_manager_->StartAdvertising();

  auto service_uuid_list =
      std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  service_uuid_list->push_back(kNearbySharingFastInitiationServiceUuid);
  EXPECT_EQ(*service_uuid_list, register_args_->service_uuids);

  auto payload = std::vector<uint8_t>(std::begin(kNearbySharingFastPairId),
                                      std::end(kNearbySharingFastPairId));
  payload.push_back(GenerateFastInitV1Metadata());
  EXPECT_EQ(
      payload,
      register_args_->service_data[kNearbySharingFastInitiationServiceUuid]);
}
