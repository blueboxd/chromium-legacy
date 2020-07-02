// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::NiceMock;
using testing::Return;

namespace {
constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};

#if defined(CHROME_OS)
const int64_t kFastInitAdvertisingInterval = 100;
const int64_t kDefaultAdvertisingInterval = 0;
#endif

}  // namespace

struct RegisterAdvertisementArgs {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdvertisement::ServiceData& service_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback)
      : service_uuids(service_uuids),
        service_data(service_data),
        callback(callback),
        error_callback(error_callback) {}

  device::BluetoothAdvertisement::UUIDList service_uuids;
  device::BluetoothAdvertisement::ServiceData service_data;
  device::BluetoothAdapter::CreateAdvertisementCallback callback;
  device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(RegisterAdvertisementArgs*));
  MOCK_METHOD2(OnSetAdvertisingInterval, void(int64_t, int64_t));

#if defined(CHROME_OS)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override {
    callback.Run();
    OnSetAdvertisingInterval(min.InMilliseconds(), max.InMilliseconds());
  }
#endif

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback) override {
    RegisterAdvertisementWithArgsStruct(new RegisterAdvertisementArgs(
        *advertisement_data->service_uuids(),
        *advertisement_data->service_data(), std::move(callback),
        std::move(error_callback)));
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
    ON_CALL(*mock_adapter_, OnSetAdvertisingInterval(_, _))
        .WillByDefault(Invoke(
            this,
            &NearbySharingFastInitiationManagerTest::OnSetAdvertisingInterval));

    fast_initiation_manager_ =
        std::make_unique<FastInitiationManager>(mock_adapter_);

    called_on_start_advertising_ = false;
    called_on_start_advertising_error_ = false;
    called_on_stop_advertising_ = false;
  }

  void OnAdapterRegisterAdvertisement(RegisterAdvertisementArgs* args) {
    register_args_ = base::WrapUnique(args);
  }

  uint8_t GenerateFastInitV1Metadata() { return 0x00; }

  void StartAdvertising() {
    fast_initiation_manager_->StartAdvertising(
        base::BindOnce(
            &NearbySharingFastInitiationManagerTest::OnStartAdvertising,
            base::Unretained(this)),
        base::BindOnce(
            &NearbySharingFastInitiationManagerTest::OnStartAdvertisingError,
            base::Unretained(this)));
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

  void StopAdvertising() {
    fast_initiation_manager_->StopAdvertising(base::BindOnce(
        &NearbySharingFastInitiationManagerTest::OnStopAdvertising,
        base::Unretained(this)));
  }

  void OnStartAdvertising() { called_on_start_advertising_ = true; }

  void OnStartAdvertisingError() { called_on_start_advertising_error_ = true; }

  void OnStopAdvertising() { called_on_stop_advertising_ = true; }

  void OnSetAdvertisingInterval(int64_t min, int64_t max) {
    ++set_advertising_interval_call_count_;
    last_advertising_interval_min_ = min;
    last_advertising_interval_max_ = max;
  }

  bool called_on_start_advertising() { return called_on_start_advertising_; }
  bool called_on_start_advertising_error() {
    return called_on_start_advertising_error_;
  }
  bool called_on_stop_advertising() { return called_on_stop_advertising_; }
  size_t set_advertising_interval_call_count() {
    return set_advertising_interval_call_count_;
  }

  int64_t last_advertising_interval_min() {
    return last_advertising_interval_min_;
  }
  int64_t last_advertising_interval_max() {
    return last_advertising_interval_max_;
  }

  scoped_refptr<NiceMock<MockBluetoothAdapterWithAdvertisements>> mock_adapter_;
  std::unique_ptr<FastInitiationManager> fast_initiation_manager_;
  std::unique_ptr<RegisterAdvertisementArgs> register_args_;
  bool called_on_start_advertising_;
  bool called_on_start_advertising_error_;
  bool called_on_stop_advertising_;
  size_t set_advertising_interval_call_count_ = 0u;
  int64_t last_advertising_interval_min_ = 0;
  int64_t last_advertising_interval_max_ = 0;
};

TEST_F(NearbySharingFastInitiationManagerTest, TestStartAdvertising_Success) {
  StartAdvertising();
  register_args_->callback.Run(
      base::MakeRefCounted<device::MockBluetoothAdvertisement>());
  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
#if defined(CHROME_OS)
  EXPECT_EQ(1u, set_advertising_interval_call_count());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_min());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_max());
#endif
}

TEST_F(NearbySharingFastInitiationManagerTest, TestStartAdvertising_Error) {
  StartAdvertising();
  register_args_->error_callback.Run(device::BluetoothAdvertisement::ErrorCode::
                                         INVALID_ADVERTISEMENT_ERROR_CODE);
  EXPECT_FALSE(called_on_start_advertising());
  EXPECT_TRUE(called_on_start_advertising_error());
  EXPECT_FALSE(called_on_stop_advertising());
#if defined(CHROME_OS)
  EXPECT_EQ(1u, set_advertising_interval_call_count());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_min());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_max());
#endif
}

TEST_F(NearbySharingFastInitiationManagerTest, TestStopAdvertising) {
  StartAdvertising();
  register_args_->callback.Run(
      base::MakeRefCounted<device::MockBluetoothAdvertisement>());
#if defined(CHROME_OS)
  EXPECT_EQ(1u, set_advertising_interval_call_count());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_min());
  EXPECT_EQ(kFastInitAdvertisingInterval, last_advertising_interval_max());
#endif

  StopAdvertising();

  EXPECT_TRUE(called_on_start_advertising());
  EXPECT_FALSE(called_on_start_advertising_error());
  EXPECT_TRUE(called_on_stop_advertising());
#if defined(CHROME_OS)
  EXPECT_EQ(2u, set_advertising_interval_call_count());
  EXPECT_EQ(kDefaultAdvertisingInterval, last_advertising_interval_min());
  EXPECT_EQ(kDefaultAdvertisingInterval, last_advertising_interval_max());
#endif
}
