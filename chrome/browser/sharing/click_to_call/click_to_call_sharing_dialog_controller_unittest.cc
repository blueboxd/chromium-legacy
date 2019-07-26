// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_sharing_dialog_controller.h"

#include <memory>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_constants.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using namespace testing;
using namespace instance_id;

namespace {

const char kPhoneNumber[] = "073%2087%202525%2078";
const char kExpectedPhoneNumber[] = "073 87 2525 78";
const char kReceiverGuid[] = "test_receiver_guid";
const char kReceiverName[] = "test_receiver_name";

class MockSharingService : public SharingService {
 public:
  explicit MockSharingService(std::unique_ptr<SharingFCMHandler> fcm_handler)
      : SharingService(/* sync_prefs= */ nullptr,
                       /* vapid_key_manager= */ nullptr,
                       /* sharing_device_registration= */ nullptr,
                       /* fcm_sender= */ nullptr,
                       std::move(fcm_handler),
                       /* gcm_driver= */ nullptr,
                       /* device_info_tracker= */ nullptr,
                       /* local_device_info_provider= */ nullptr,
                       /* sync_service */ nullptr) {}

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
  ClickToCallSharingDialogControllerTest() = default;

  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSharingService>>(
              std::make_unique<SharingFCMHandler>(nullptr, nullptr));
        }));
    ClickToCallSharingDialogController::ShowDialog(
        web_contents_.get(), GURL(base::StrCat({"tel:", kPhoneNumber})), false);
    click_to_call_sharing_dialog_controller_ =
        ClickToCallSharingDialogController::GetOrCreateFromWebContents(
            web_contents_.get());
  }

 protected:
  NiceMock<MockSharingService>* service() {
    return static_cast<NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(&profile_));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  ClickToCallSharingDialogController* click_to_call_sharing_dialog_controller_ =
      nullptr;
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
  SharingDeviceInfo sharing_device_info(
      kReceiverGuid, base::UTF8ToUTF16(kReceiverName),
      sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(), 1);
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      kExpectedPhoneNumber);
  EXPECT_CALL(*service(), SendMessageToDevice(Eq(kReceiverGuid),
                                              Eq(kSharingClickToCallMessageTTL),
                                              ProtoEquals(sharing_message), _));
  click_to_call_sharing_dialog_controller_->OnDeviceChosen(sharing_device_info);
}

// Check the call to sharing service to get all synced devices.
TEST_F(ClickToCallSharingDialogControllerTest, GetSyncedDevices) {
  EXPECT_CALL(*service(), GetDeviceCandidates(Eq(static_cast<int>(
                              SharingDeviceCapability::kTelephony))));
  click_to_call_sharing_dialog_controller_->GetSyncedDevices();
}
