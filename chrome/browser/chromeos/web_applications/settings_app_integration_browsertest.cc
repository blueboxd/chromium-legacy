// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

class SettingsAppIntegrationTest : public SystemWebAppIntegrationTest {};

// Test that the Settings App installs and launches correctly.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsApp) {
  const GURL url("chrome://os-settings");
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::SETTINGS, url, "Settings"));
}

// Test that the Settings App installs correctly when it's set to be disabled
// via SystemFeaturesDisableList policy, but doesn't launch.
IN_PROC_BROWSER_TEST_P(SettingsAppIntegrationTest, SettingsAppDisabled) {
  {
    ListPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                          policy::policy_prefs::kSystemFeaturesDisableList);
    base::ListValue* list = update.Get();
    list->Append(policy::SystemFeature::OS_SETTINGS);
  }

  ASSERT_FALSE(GetManager()
                   .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS)
                   .has_value());
  Browser* app_browser =
      WaitForSystemAppInstallAndLaunch(web_app::SystemAppType::SETTINGS);
  ASSERT_TRUE(GetManager()
                  .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS)
                  .has_value());

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  observer.WaitForNavigationFinished();
  content::WebUI* web_ui = web_contents->GetCommittedWebUI();
  ASSERT_FALSE(web_ui);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SettingsAppIntegrationTest,
                         ::testing::Values(web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
