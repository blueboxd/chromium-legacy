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
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_device_registration_result.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace {
const char kTelUrl[] = "tel:+9876543210";
const char kNonTelUrl[] = "https://google.com";

const char kTextWithPhoneNumber[] = "call 9876543210 now";
const char kTextWithoutPhoneNumber[] = "abcde";
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
    host_resolver()->AddRule("mock.http", "127.0.0.1");
  }

  void Init(const std::vector<base::Feature>& enabled_features,
            const std::vector<base::Feature>& disabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("mock.http", "/sharing/tel.html");
    ASSERT_TRUE(sessions_helper::OpenTab(0, url));

    web_contents_ = GetBrowser(0)->tab_strip_model()->GetWebContentsAt(0);

    // Ensure that the test page has loaded.
    const base::string16 title = base::ASCIIToUTF16("Tel");
    content::TitleWatcher watcher(web_contents_, title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());

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
          std::set<sync_pb::SharingSpecificFields_EnabledFeatures>{
              sync_pb::SharingSpecificFields::CLICK_TO_CALL},
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
  std::unique_ptr<TestRenderViewContextMenu> InitRightClickMenu(
      const GURL& url,
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

  void GetDeviceFCMToken(const std::string& guid,
                         std::string* fcm_token) const {
    auto sharing_info =
        sharing_service_->GetSyncPreferences()->GetSharingInfo(guid);
    ASSERT_TRUE(sharing_info);
    *fcm_token = sharing_info->fcm_token;
  }

  void CheckLastSharingMessageSent(
      const std::string& fcm_token,
      const std::string& expected_phone_number) const {
    EXPECT_EQ(fcm_token, gcm_service_->last_receiver_id());
    chrome_browser_sharing::SharingMessage sharing_message;
    sharing_message.ParseFromString(
        gcm_service_->last_web_push_message().payload);
    ASSERT_TRUE(sharing_message.has_click_to_call_message());
    EXPECT_EQ(expected_phone_number,
              sharing_message.click_to_call_message().phone_number());
  }

  SharingService* sharing_service() const { return sharing_service_; }

  content::WebContents* web_contents() const { return web_contents_; }

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
                       ContextMenu_TelLink_SingleDeviceAvailable) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  SetUpDevices(/*count=*/1);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));

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
  CheckLastSharingMessageSent(fcm_token, GetUnescapedURLContent(GURL(kTelUrl)));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_NoDevicesAvailable) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  AwaitQuiescence();

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_DevicesAvailable_SyncTurnedOff) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  SetUpDevices(/*count=*/1);
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_TelLink_MultipleDevicesAvailable) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithoutPhoneNumber));
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
    CheckLastSharingMessageSent(fcm_token,
                                GetUnescapedURLContent(GURL(kTelUrl)));
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest,
                       ContextMenu_HighlightedText_MultipleDevicesAvailable) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));
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
    base::Optional<std::string> expected_number =
        ExtractPhoneNumberForClickToCall(GetProfile(0), kTextWithPhoneNumber);
    ASSERT_TRUE(expected_number.has_value());
    CheckLastSharingMessageSent(fcm_token, expected_number.value());
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(
    ClickToCallBrowserTest,
    ContextMenu_HighlightedText_DevicesAvailable_FeatureFlagOff) {
  Init({kSharingDeviceRegistration, kClickToCallUI},
       {kClickToCallContextMenuForSelectedText});
  SetUpDevices(/*count=*/2);

  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::CLICK_TO_CALL);

  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));

  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  EXPECT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, ContextMenu_UKM) {
  Init({kSharingDeviceRegistration, kClickToCallUI,
        kClickToCallContextMenuForSelectedText},
       {});
  SetUpDevices(/*count=*/1);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::RunLoop run_loop;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::Sharing_ClickToCall::kEntryName, run_loop.QuitClosure());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitRightClickMenu(GURL(kNonTelUrl), base::ASCIIToUTF16("Google"),
                         base::ASCIIToUTF16(kTextWithPhoneNumber));

  // Check click to call items in context menu
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE));
  // Send number to device
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);

  // Expect UKM metrics to be logged
  run_loop.Run();
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::Sharing_ClickToCall::kEntryName);
  ASSERT_EQ(1u, ukm_entries.size());

  const int64_t* entry_point = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kEntryPointName);
  const int64_t* has_apps = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasAppsName);
  const int64_t* has_devices = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kHasDevicesName);
  const int64_t* selection = ukm_recorder.GetEntryMetric(
      ukm_entries[0], ukm::builders::Sharing_ClickToCall::kSelectionName);

  ASSERT_TRUE(entry_point);
  ASSERT_TRUE(has_apps);
  ASSERT_TRUE(has_devices);
  ASSERT_TRUE(selection);

  EXPECT_EQ(
      static_cast<int64_t>(SharingClickToCallEntryPoint::kRightClickSelection),
      *entry_point);
  EXPECT_EQ(true, *has_devices);
  EXPECT_EQ(static_cast<int64_t>(SharingClickToCallSelection::kDevice),
            *selection);
  // TODO(knollr): mock apps and verify |has_apps| here too.
}

IN_PROC_BROWSER_TEST_F(ClickToCallBrowserTest, CloseTabWithBubble) {
  Init({kSharingDeviceRegistration, kClickToCallUI}, {});
  SetUpDevices(/*count=*/1);

  base::RunLoop run_loop;
  ClickToCallUiController::GetOrCreateFromWebContents(web_contents())
      ->set_on_dialog_shown_closure_for_testing(run_loop.QuitClosure());

  // Click on the tel link to trigger the bubble view.
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("document.querySelector('a').click();"),
      base::NullCallback());
  // Wait until the bubble is visible.
  run_loop.Run();

  // Close the tab while the bubble is opened.
  // Regression test for http://crbug.com/1000934.
  sessions_helper::CloseTab(/*browser_index=*/0, /*tab_index=*/0);
}
