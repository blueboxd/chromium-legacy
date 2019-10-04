// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"

#include <memory>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/fake_local_device_info_provider.h"
#include "chrome/browser/sharing/sharing_constants.h"
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
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using namespace testing;
using namespace instance_id;

namespace {

const char kText[] = "Text to be copied";
const char kExpectedText[] = "Text to be copied";
const char kReceiverGuid[] = "test_receiver_guid";
const char kReceiverName[] = "test_receiver_name";

class MockSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  explicit MockSharingDeviceRegistration()
      : SharingDeviceRegistration(/* pref_service_= */ nullptr,
                                  /* sharing_sync_preference_= */ nullptr,
                                  /* instance_id_driver_= */ nullptr,
                                  /* vapid_key_manager_= */ nullptr) {}

  ~MockSharingDeviceRegistration() override = default;

  MOCK_CONST_METHOD0(IsSharedClipboardSupported, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingDeviceRegistration);
};

class MockSharingService : public SharingService {
 public:
  explicit MockSharingService(std::unique_ptr<SharingFCMHandler> fcm_handler)
      : SharingService(/* sync_prefs= */ nullptr,
                       /* vapid_key_manager= */ nullptr,
                       std::make_unique<MockSharingDeviceRegistration>(),
                       /* fcm_sender= */ nullptr,
                       std::move(fcm_handler),
                       /* gcm_driver= */ nullptr,
                       /* device_info_tracker= */ nullptr,
                       /* local_device_info_provider= */ nullptr,
                       /* sync_service */ nullptr,
                       /* notification_display_service= */ nullptr) {}

  ~MockSharingService() override = default;

  MOCK_CONST_METHOD1(
      GetDeviceCandidates,
      std::vector<std::unique_ptr<syncer::DeviceInfo>>(
          sync_pb::SharingSpecificFields::EnabledFeatures required_feature));

  MOCK_METHOD4(SendMessageToDevice,
               void(const std::string& device_guid,
                    base::TimeDelta time_to_live,
                    chrome_browser_sharing::SharingMessage message,
                    SharingService::SendMessageCallback callback));
};

class SharedClipboardUiControllerTest : public testing::Test {
 public:
  SharedClipboardUiControllerTest() = default;

  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSharingService>>(
              std::make_unique<SharingFCMHandler>(nullptr, nullptr, nullptr));
        }));
    syncer::DeviceInfo device_info(
        kReceiverGuid, kReceiverName, "chrome_version", "user_agent",
        sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
        /*last_updated_timestamp=*/base::Time::Now(),
        /*send_tab_to_self_receiving_enabled=*/false,
        /*sharing_info=*/base::nullopt);
    controller_ = SharedClipboardUiController::GetOrCreateFromWebContents(
        web_contents_.get());
    controller_->OnDeviceSelected(base::UTF8ToUTF16(kText), device_info);
  }

 protected:
  NiceMock<MockSharingService>* service() {
    return static_cast<NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(&profile_));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  SharedClipboardUiController* controller_ = nullptr;
};
}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Check the call to sharing service when a device is chosen.
TEST_F(SharedClipboardUiControllerTest, OnDeviceChosen) {
  syncer::DeviceInfo device_info(
      kReceiverGuid, kReceiverName, "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*sharing_info=*/base::nullopt);

  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(kExpectedText);
  EXPECT_CALL(*service(),
              SendMessageToDevice(Eq(kReceiverGuid), Eq(kSharingMessageTTL),
                                  ProtoEquals(sharing_message), _));
  controller_->OnDeviceChosen(device_info);
}

// Check the call to sharing service to get all synced devices.
TEST_F(SharedClipboardUiControllerTest, GetSyncedDevices) {
  EXPECT_CALL(*service(),
              GetDeviceCandidates(
                  Eq(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD)));
  controller_->UpdateAndShowDialog();
}
