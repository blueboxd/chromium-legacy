// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "components/location/android/location_settings_dialog_outcome.h"
#include "components/location/android/mock_location_settings.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#endif

#if defined(OS_ANDROID)
namespace {
constexpr char kDSETestUrl[] = "https://www.dsetest.com";

class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  base::string16 GetDSEName() override { return base::string16(); }

  url::Origin GetDSEOrigin() override {
    return url::Origin::Create(GURL(kDSETestUrl));
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  void UpdateDSEOrigin() { dse_changed_callback_.Run(); }

 private:
  base::Closure dse_changed_callback_;
};
}  // namespace
#endif

class GeolocationPermissionContextDelegateTests
    : public ChromeRenderViewHostTestHarness {
 protected:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    content_settings::TabSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::TabSpecificContentSettingsDelegate>(
            web_contents()));
#if defined(OS_ANDROID)
    static_cast<permissions::GeolocationPermissionContextAndroid*>(
        PermissionManagerFactory::GetForProfile(profile())
            ->GetPermissionContextForTesting(ContentSettingsType::GEOLOCATION))
        ->SetLocationSettingsForTesting(
            std::make_unique<MockLocationSettings>());
    MockLocationSettings::SetLocationStatus(true, true);
    MockLocationSettings::SetCanPromptForAndroidPermission(true);
    MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                          GRANTED);
    MockLocationSettings::ClearHasShownLocationSettingsDialog();
#endif
  }

  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting) {
    content_settings::TabSpecificContentSettings* content_settings =
        content_settings::TabSpecificContentSettings::FromWebContents(
            web_contents());
    const ContentSettingsUsagesState::StateMap& state_map =
        content_settings->geolocation_usages_state().state_map();
    EXPECT_EQ(1U, state_map.count(requesting_frame.GetOrigin()));
    EXPECT_EQ(0U, state_map.count(requesting_frame));
    auto settings = state_map.find(requesting_frame.GetOrigin());
    ASSERT_FALSE(settings == state_map.end())
        << "geolocation state not found " << requesting_frame;
    EXPECT_EQ(expected_content_setting, settings->second);
  }
};

TEST_F(GeolocationPermissionContextDelegateTests, TabContentSettingIsUpdated) {
  GURL requesting_frame("https://www.example.com/geolocation");
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  permissions::MockPermissionPromptFactory mock_prompt_factory(manager);
  NavigateAndCommit(requesting_frame);
  manager->DocumentOnLoadCompletedInMainFrame();

  base::RunLoop run_loop;
  PermissionManagerFactory::GetForProfile(profile())->RequestPermission(
      ContentSettingsType::GEOLOCATION, main_rfh(), requesting_frame, true,
      base::BindOnce(
          [](base::RunLoop* run_loop, ContentSetting content_setting) {
            EXPECT_EQ(content_setting, CONTENT_SETTING_ALLOW);
            run_loop->Quit();
          },
          &run_loop));
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(manager->IsRequestInProgress());
  manager->Accept();
  run_loop.Run();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
}

#if defined(OS_ANDROID)
TEST_F(GeolocationPermissionContextDelegateTests,
       SearchGeolocationInIncognito) {
  GURL requesting_frame(kDSETestUrl);

  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(profile());
  std::unique_ptr<TestSearchEngineDelegate> delegate =
      std::make_unique<TestSearchEngineDelegate>();
  TestSearchEngineDelegate* delegate_ptr = delegate.get();
  service->SetSearchEngineDelegateForTest(std::move(delegate));
  delegate_ptr->UpdateDSEOrigin();

  // The DSE should be auto-granted geolocation.
  ASSERT_EQ(CONTENT_SETTING_ALLOW,
            PermissionManagerFactory::GetForProfile(profile())
                ->GetPermissionStatus(ContentSettingsType::GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);

  Profile* otr_profile = profile()->GetOffTheRecordProfile();

  // A DSE setting of ALLOW should not flow through to incognito.
  ASSERT_EQ(CONTENT_SETTING_ASK,
            PermissionManagerFactory::GetForProfile(otr_profile)
                ->GetPermissionStatus(ContentSettingsType::GEOLOCATION,
                                      requesting_frame, requesting_frame)
                .content_setting);
}
#endif
