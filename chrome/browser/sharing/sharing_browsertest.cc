// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_browsertest.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "components/sync/driver/profile_sync_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

SharingBrowserTest::SharingBrowserTest()
    : SyncTest(TWO_CLIENT),
      scoped_testing_factory_installer_(
          base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

SharingBrowserTest::~SharingBrowserTest() = default;

void SharingBrowserTest::SetUpOnMainThread() {
  SyncTest::SetUpOnMainThread();
  host_resolver()->AddRule("mock.http", "127.0.0.1");
}

void SharingBrowserTest::Init(
    const std::vector<base::Feature>& enabled_features,
    const std::vector<base::Feature>& disabled_features) {
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("mock.http", GetTestPageURL());
  ASSERT_TRUE(sessions_helper::OpenTab(0, url));

  web_contents_ = GetBrowser(0)->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(NavigateToURL(web_contents_, url));

  gcm_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetForProfile(GetProfile(0)));
  gcm_service_->set_collect(true);

  sharing_service_ = SharingServiceFactory::GetForBrowserContext(GetProfile(0));
}

void SharingBrowserTest::SetUpDevices(
    int count,
    sync_pb::SharingSpecificFields_EnabledFeatures feature) {
  for (int i = 0; i < count; i++) {
    SharingService* service =
        SharingServiceFactory::GetForBrowserContext(GetProfile(i));
    service->SetDeviceInfoTrackerForTesting(&fake_device_info_tracker_);

    base::RunLoop run_loop;
    service->RegisterDeviceInTesting(
        std::set<sync_pb::SharingSpecificFields_EnabledFeatures>{feature},
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
            device->send_tab_to_self_receiving_enabled(),
            device->sharing_info());
    fake_device_info_tracker_.Add(fake_device.get());
    device_infos_.push_back(std::move(fake_device));
  }
}

// TODO(himanshujaju): try to move to static method in
// render_view_context_menu_test_util.cc
std::unique_ptr<TestRenderViewContextMenu>
SharingBrowserTest::InitRightClickMenu(const GURL& url,
                                       const base::string16& link_text,
                                       const base::string16& selection_text) {
  content::ContextMenuParams params;
  params.selection_text = selection_text;
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

void SharingBrowserTest::CheckLastReceiver(
    const std::string& device_guid) const {
  auto sharing_info =
      sharing_service_->GetSyncPreferences()->GetSharingInfo(device_guid);
  ASSERT_TRUE(sharing_info);
  EXPECT_EQ(sharing_info->fcm_token, gcm_service_->last_receiver_id());
}

chrome_browser_sharing::SharingMessage
SharingBrowserTest::GetLastSharingMessageSent() const {
  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.ParseFromString(
      gcm_service_->last_web_push_message().payload);
  return sharing_message;
}

SharingService* SharingBrowserTest::sharing_service() const {
  return sharing_service_;
}

content::WebContents* SharingBrowserTest::web_contents() const {
  return web_contents_;
}
