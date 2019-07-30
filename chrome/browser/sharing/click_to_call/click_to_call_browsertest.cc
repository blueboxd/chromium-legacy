// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_device_info.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
}  // namespace

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
    web_contents = web_contents_ptr.get();
    Browser* browser = AddBrowser(0);
    browser->tab_strip_model()->AppendWebContents(std::move(web_contents_ptr),
                                                  true);
  }

  content::WebContents* GetWebContents() { return web_contents; }

  void SetUpDevices(int count) {
    for (int i = 0; i < count; i++) {
      SharingService* service =
          SharingServiceFactory::GetForBrowserContext(GetProfile(i));

      base::RunLoop run_loop;
      service->RegisterDeviceInTesting(
          std::string("TEST"),
          static_cast<int>(SharingDeviceCapability::kTelephony),
          base::BindLambdaForTesting([&](SharingDeviceRegistrationResult r) {
            ASSERT_EQ(SharingDeviceRegistrationResult::kSuccess, r);
            run_loop.Quit();
          }));
      run_loop.Run();
    }
  }

  std::unique_ptr<TestRenderViewContextMenu> InitRightClickMenu(
      content::WebContents* web_contents,
      const GURL& url,
      const base::string16& link_text) {
    content::ContextMenuParams params;
    params.media_type = blink::WebContextMenuData::MediaType::kMediaTypeNone;
    params.unfiltered_link_url = url;
    params.link_url = url;
    params.src_url = url;
    params.link_text = link_text;
    params.page_url = web_contents->GetVisibleURL();
    params.source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
#if defined(OS_MACOSX)
    params.writing_direction_default = 0;
    params.writing_direction_left_to_right = 0;
    params.writing_direction_right_to_left = 0;
#endif
    auto menu = std::make_unique<TestRenderViewContextMenu>(
        web_contents->GetMainFrame(), params);
    menu->Init();
    return menu;
  }

  void GetDeviceFCMToken(SharingService* sharing_service,
                         const std::string& guid,
                         std::string* fcm_token) const {
    auto devices = sharing_service->GetSyncPreferences()->GetSyncedDevices();
    auto it = devices.find(guid);
    ASSERT_NE(devices.end(), it);
    *fcm_token = it->second.fcm_token;
  }

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::WebContents* web_contents;
  std::vector<std::string> device_fcm_tokens_;
  DISALLOW_COPY_AND_ASSIGN(ClickToCallBrowserTest);
};

// TODO(himanshujaju): Add UI checks. Modularize common functions.
IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_SingleDeviceAvailable) {
  SetUpDevices(1);
  AwaitQuiescence();

  gcm::FakeGCMProfileService* gcm_service =
      static_cast<gcm::FakeGCMProfileService*>(
          gcm::GCMProfileServiceFactory::GetForProfile(GetProfile(0)));
  gcm_service->set_collect(true);

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(GetProfile(0));
  auto devices = sharing_service->GetDeviceCandidates(
      static_cast<int>(SharingDeviceCapability::kTelephony));

  std::unique_ptr<TestRenderViewContextMenu> menu = InitRightClickMenu(
      GetWebContents(), GURL(kTelUrl), base::ASCIIToUTF16("Google"));

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));

  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  // Check SharingMessage and Receiver id
  std::string fcm_token;
  GetDeviceFCMToken(sharing_service, devices[0].guid(), &fcm_token);
  EXPECT_EQ(fcm_token, gcm_service->last_receiver_id());
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.ParseFromString(gcm_service->last_web_push_message().payload);
  ASSERT_TRUE(sharing_message.has_click_to_call_message());
  EXPECT_EQ(GURL(kTelUrl).GetContent(),
            sharing_message.click_to_call_message().phone_number());
}
