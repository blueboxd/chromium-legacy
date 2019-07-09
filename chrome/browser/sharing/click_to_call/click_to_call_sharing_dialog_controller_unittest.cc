// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"

#include <memory>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_constants.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace testing;
using namespace instance_id;

namespace {

const char kPhoneNumber[] = "987654321";
const char kReceiverGuid[] = "test_receiver_guid";
const char kReceiverName[] = "test_receiver_name";

class MockSharingService : public SharingService {
 public:
  MockSharingService(syncer::SyncService* sync_service,
                              std::unique_ptr<SharingFCMHandler> fcm_handler)
      : SharingService(nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       std::move(fcm_handler),
                       nullptr,
                       sync_service) {}

  ~MockSharingService() override = default;

  MOCK_CONST_METHOD1(GetDeviceCandidates,
                     std::vector<SharingDeviceInfo>(int required_capabilities));

  MOCK_METHOD4(SendMessageToDevice,
               void(const std::string& device_guid,
                    base::TimeDelta time_to_live,
                    chrome_browser_sharing::SharingMessage message,
                    SharingService::SendMessageCallback callback));
};

class ClickToCallSharingDialogControllerTest : public testing::Test {
 public:
  ClickToCallSharingDialogControllerTest() {}

  syncer::FakeSyncService fake_sync_service_;

  NiceMock<MockSharingService> mock_sharing_service_{
      &fake_sync_service_,
      std::make_unique<SharingFCMHandler>(nullptr, nullptr)};

  ClickToCallSharingDialogController click_to_call_sharing_dialog_controller_{
      nullptr, &mock_sharing_service_, kPhoneNumber};
};
}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Check the call to sharing service when a device is chosen.
TEST_F(ClickToCallSharingDialogControllerTest, OnDeviceChosen) {
  SharingDeviceInfo sharing_device_info(kReceiverGuid, kReceiverName,
                                        sync_pb::SyncEnums::TYPE_PHONE,
                                        base::Time::Now(), 1);
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      kPhoneNumber);
  SharingService::SendMessageCallback callback;
  EXPECT_CALL(
      mock_sharing_service_,
      SendMessageToDevice(Eq(kReceiverGuid), Eq(kSharingClickToCallMessageTTL),
                          ProtoEquals(sharing_message), _));
  click_to_call_sharing_dialog_controller_.OnDeviceChosen(sharing_device_info,
                                                          std::move(callback));
}

// Check the call to sharing service to get all synced devices.
TEST_F(ClickToCallSharingDialogControllerTest, GetSyncedDevices) {
  EXPECT_CALL(mock_sharing_service_,
              GetDeviceCandidates(
                  Eq(static_cast<int>(SharingDeviceCapability::kTelephony))));
  click_to_call_sharing_dialog_controller_.GetSyncedDevices();
}
