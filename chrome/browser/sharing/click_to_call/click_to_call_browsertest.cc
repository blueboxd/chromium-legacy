// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_capability.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
}  // namespace

// TODO(himanshujaju): refactor out SharingBrowserTest to be reused by other
// features.
class ClickToCallBrowserTest : public SyncTest {
 public:
  ClickToCallBrowserTest()
      : SyncTest(TWO_CLIENT),
        scoped_testing_factory_installer_(
            base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

  ~ClickToCallBrowserTest() override {}

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    scoped_feature_list_.InitWithFeatures(
        {kClickToCallUI, kSharingDeviceRegistration}, {});

    ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

    std::unique_ptr<content::WebContents> web_contents_ptr =
        content::WebContents::Create(
            content::WebContents::CreateParams(GetProfile(0)));
    web_contents_ = web_contents_ptr.get();
    Browser* browser = AddBrowser(0);
    browser->tab_strip_model()->AppendWebContents(std::move(web_contents_ptr),
                                                  true);

    gcm_service_ = static_cast<gcm::FakeGCMProfileService*>(
        gcm::GCMProfileServiceFactory::GetForProfile(GetProfile(0)));
    gcm_service_->set_collect(true);

    sharing_service_ =
        SharingServiceFactory::GetForBrowserContext(GetProfile(0));
  }

  void SetUpDevices(int count) {
    for (int i = 0; i < count; i++) {
      SharingService* service =
          SharingServiceFactory::GetForBrowserContext(GetProfile(i));
      service->SetDeviceInfoTrackerForTesting(&fake_device_info_tracker_);

      base::RunLoop run_loop;
      service->RegisterDeviceInTesting(
          static_cast<int>(SharingDeviceCapability::kTelephony),
          base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
            ASSERT_EQ(SharingDeviceRegistrationResult::kSuccess, r);
            run_loop.Quit();
          }));
      run_loop.Run();
      AwaitQuiescence();
    }

    syncer::DeviceInfoTracker* original_device_info_tracker =
        DeviceInfoSyncServiceFactory::GetForProfile(GetProfile(0))
            ->GetDeviceInfoTracker();
    std::vector<std::unique_ptr<syncer::DeviceInfo>> original_devices =
        original_device_info_tracker->GetAllDeviceInfo();
    int device_id = 0;

    for (auto& device : original_devices) {
      std::unique_ptr<syncer::DeviceInfo> fake_device =
          std::make_unique<syncer::DeviceInfo>(
              device->guid(),
              base::StrCat(
                  {"testing_device_", base::NumberToString(device_id++)}),
              device->chrome_version(), device->sync_user_agent(),
              device->device_type(), device->signin_scoped_device_id(),
              device->last_updated_timestamp(),
              device->send_tab_to_self_receiving_enabled());
      fake_device_info_tracker_.Add(fake_device.get());
      device_infos_.push_back(std::move(fake_device));
    }
  }

  // TODO(himanshujaju): try to move to static method in
  // render_view_context_menu_test_util.cc
  std::unique_ptr<TestRenderViewContextMenu> InitRightClickMenu(
      const GURL& url,
      const base::string16& link_text) {
    content::ContextMenuParams params;
    params.media_type = blink::WebContextMenuData::MediaType::kMediaTypeNone;
    params.unfiltered_link_url = url;
    params.link_url = url;
    params.src_url = url;
    params.link_text = link_text;
    params.page_url = web_contents_->GetVisibleURL();
    params.source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        web_contents_->GetMainFrame(), params);
    menu->Init();
    return menu;
  }

  void GetDeviceFCMToken(const std::string& guid,
                         std::string* fcm_token) const {
    auto devices = sharing_service_->GetSyncPreferences()->GetSyncedDevices();
    auto it = devices.find(guid);
    ASSERT_NE(devices.end(), it);
    *fcm_token = it->second.fcm_token;
  }

  void CheckLastSharingMessageSent(const std::string& fcm_token,
                                   const GURL& url) const {
    EXPECT_EQ(fcm_token, gcm_service_->last_receiver_id());
    chrome_browser_sharing::SharingMessage sharing_message;
    sharing_message.ParseFromString(
        gcm_service_->last_web_push_message().payload);
    ASSERT_TRUE(sharing_message.has_click_to_call_message());
    EXPECT_EQ(url.GetContent(),
              sharing_message.click_to_call_message().phone_number());
  }

  SharingService* sharing_service() const { return sharing_service_; }

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  gcm::FakeGCMProfileService* gcm_service_;
  content::WebContents* web_contents_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos_;
  SharingService* sharing_service_;
  DISALLOW_COPY_AND_ASSIGN(ClickToCallBrowserTest);
};

// TODO(himanshujaju): Add UI checks.
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_SingleDeviceAvailable) {
  SetUpDevices(/*count=*/1);

  auto devices = sharing_service()->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));

  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"));

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  // Check fcm token and message sent.
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
  std::string fcm_token;
  GetDeviceFCMToken(devices[0]->guid(), &fcm_token);
  CheckLastSharingMessageSent(fcm_token, GURL(kTelUrl));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_NoDevicesAvailable) {
  AwaitQuiescence();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_DevicesAvailable_SyncTurnedOff) {
  SetUpDevices(/*count=*/1);
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_MultipleDevicesAvailable) {
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    std::string fcm_token;
    GetDeviceFCMToken(device->guid(), &fcm_token);
    CheckLastSharingMessageSent(fcm_token, GURL(kTelUrl));
    device_id++;
  }
}
