// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_constants.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using SharingMessage = chrome_browser_sharing::SharingMessage;

namespace {

const char kTelUrl[] = "tel:+9876543210";

constexpr int kSeparatorCommandId = -1;

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
                    SharingMessage message,
                    SharingService::SendMessageCallback callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharingService);
};

class ClickToCallContextMenuObserverTest : public testing::Test {
 public:
  ClickToCallContextMenuObserverTest() = default;

  ~ClickToCallContextMenuObserverTest() override = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        menu_.GetBrowserContext(), nullptr);
    menu_.set_web_contents(web_contents_.get());
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        menu_.GetBrowserContext(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSharingService>>(
              std::make_unique<SharingFCMHandler>(nullptr, nullptr));
        }));
    observer_ = std::make_unique<ClickToCallContextMenuObserver>(&menu_);
    menu_.SetObserver(observer_.get());
  }

  void InitMenu(const GURL& url) {
    content::ContextMenuParams params;
    params.link_url = url;
    observer_->InitMenu(params);
    sharing_message.mutable_click_to_call_message()->set_phone_number(
        url.GetContent());
  }

  std::vector<SharingDeviceInfo> CreateMockDevices(int count) {
    std::vector<SharingDeviceInfo> devices;
    for (int i = 0; i < count; i++) {
      devices.emplace_back(
          base::StrCat({"guid", base::NumberToString(i)}),
          base::UTF8ToUTF16(base::StrCat({"name", base::NumberToString(i)})),
          sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(),
          static_cast<int>(SharingDeviceCapability::kTelephony));
    }
    return devices;
  }

 protected:
  NiceMock<MockSharingService>* service() {
    return static_cast<NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(menu_.GetBrowserContext()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  MockRenderViewContextMenu menu_{/* incognito= */ false};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ClickToCallContextMenuObserver> observer_;
  SharingMessage sharing_message;

  DISALLOW_COPY_AND_ASSIGN(ClickToCallContextMenuObserverTest);
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST_F(ClickToCallContextMenuObserverTest, NoDevices_DoNotShowMenu) {
  auto devices = CreateMockDevices(0);

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(GURL(kTelUrl));

  EXPECT_EQ(0U, menu_.GetMenuSize());
}

TEST_F(ClickToCallContextMenuObserverTest, SingleDevice_ShowMenu) {
  auto devices = CreateMockDevices(1);
  auto guid = devices[0].guid();

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(GURL(kTelUrl));

  // The first item is a separator and the second item is the device.
  EXPECT_EQ(2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            item.command_id);

  // Emulate click on the device.
  EXPECT_CALL(*service(),
              SendMessageToDevice(Eq(guid), Eq(kSharingClickToCallMessageTTL),
                                  ProtoEquals(sharing_message), _))
      .Times(1);
  menu_.ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
}

TEST_F(ClickToCallContextMenuObserverTest, MultipleDevices_ShowMenu) {
  constexpr int device_count = 3;
  auto devices = CreateMockDevices(device_count);
  std::vector<std::string> guids;
  for (auto& device : devices)
    guids.push_back(device.guid());

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(GURL(kTelUrl));

  EXPECT_EQ(device_count + 2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < device_count; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 2, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all commands to check for commands with no device
  // assigned.
  for (int i = 0; i < kMaxDevicesShown; i++) {
    if (i < device_count) {
      EXPECT_CALL(
          *service(),
          SendMessageToDevice(Eq(guids[i]), Eq(kSharingClickToCallMessageTTL),
                              ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}

TEST_F(ClickToCallContextMenuObserverTest,
       MultipleDevices_MoreThanMax_ShowMenu) {
  int device_count = kMaxDevicesShown + 1;
  auto devices = CreateMockDevices(device_count);
  std::vector<std::string> guids;
  for (auto& device : devices)
    guids.push_back(device.guid());

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(GURL(kTelUrl));

  EXPECT_EQ(kMaxDevicesShown + 2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < kMaxDevicesShown; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 2, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all device commands to check for commands outside valid
  // range too.
  for (int i = 0; i < device_count; i++) {
    if (i < kMaxDevicesShown) {
      EXPECT_CALL(
          *service(),
          SendMessageToDevice(Eq(guids[i]), Eq(kSharingClickToCallMessageTTL),
                              ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}
