// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_delegate.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const GURL kAppUrl1(content::GetWebUIURL("system-app1"));
const GURL kAppUrl2(content::GetWebUIURL("system-app2"));
const GURL kAppUrl3(content::GetWebUIURL("system-app3"));

InstallOptions GetWindowedInstallOptions() {
  InstallOptions options(kAppUrl1, LaunchContainer::kWindow,
                         InstallSource::kSystemInstalled);
  options.add_to_applications_menu = false;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.bypass_service_worker_check = true;
  options.always_update = true;
  return options;
}

}  // namespace

class SystemWebAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SystemWebAppManagerTest()
      : test_web_app_provider_creator_(
            base::BindOnce(&SystemWebAppManagerTest::CreateWebAppProvider,
                           base::Unretained(this))) {
    scoped_feature_list_.InitWithFeatures({features::kSystemWebApps}, {});
  }

  ~SystemWebAppManagerTest() override = default;

  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile) {
    auto provider = std::make_unique<TestWebAppProvider>(profile);

    auto test_pending_app_manager = std::make_unique<TestPendingAppManager>();
    test_pending_app_manager_ = test_pending_app_manager.get();
    provider->SetPendingAppManager(std::move(test_pending_app_manager));

    auto system_web_app_manager = std::make_unique<TestSystemWebAppManager>(
        profile, test_pending_app_manager_);
    system_web_app_manager_ = system_web_app_manager.get();
    provider->SetSystemWebAppManager(std::move(system_web_app_manager));

    return provider;
  }

  void SimulatePreviouslyInstalledApp(
      GURL url,
      InstallSource install_source) {
    std::string id =
        crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(url, id, install_source);

    pending_app_manager()->SimulatePreviouslyInstalledApp(url, install_source);
  }

 protected:
  TestPendingAppManager* pending_app_manager() {
    return test_pending_app_manager_;
  }

  TestSystemWebAppManager* system_web_app_manager() {
    return system_web_app_manager_;
  }

  TestWebAppUiDelegate* ui_delegate() { return &ui_delegate_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestWebAppProviderCreator test_web_app_provider_creator_;
  TestPendingAppManager* test_pending_app_manager_ = nullptr;
  TestSystemWebAppManager* system_web_app_manager_ = nullptr;
  TestWebAppUiDelegate ui_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerTest);
};

// Test that System Apps are uninstalled with the feature disabled.
TEST_F(SystemWebAppManagerTest, Disabled) {
  base::test::ScopedFeatureList disable_feature_list;
  disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

  SimulatePreviouslyInstalledApp(kAppUrl1, InstallSource::kSystemInstalled);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps[SystemAppType::SETTINGS] = {kAppUrl1};

  system_web_app_manager()->SetSystemApps(std::move(system_apps));
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pending_app_manager()->install_requests().empty());

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({kAppUrl1}),
            pending_app_manager()->uninstall_requests());
}

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps[SystemAppType::SETTINGS] = {kAppUrl1};
  system_apps[SystemAppType::DISCOVER] = {kAppUrl2};

  system_web_app_manager()->SetSystemApps(std::move(system_apps));
  system_web_app_manager()->Start(ui_delegate());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager()->install_requests();
  EXPECT_FALSE(apps_to_install.empty());
}

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(kAppUrl1, InstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(kAppUrl2, InstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(kAppUrl3, InstallSource::kInternal);
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps[SystemAppType::SETTINGS] = {kAppUrl1};

  system_web_app_manager()->SetSystemApps(std::move(system_apps));
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the System App list.
  std::vector<InstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  EXPECT_EQ(pending_app_manager()->install_requests(),
            expected_install_options_list);

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({kAppUrl2}),
            pending_app_manager()->uninstall_requests());
}

TEST_F(SystemWebAppManagerTest, AlwaysUpdate) {
  system_web_app_manager()->SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps[SystemAppType::SETTINGS] = {kAppUrl1};
  system_web_app_manager()->SetSystemApps(system_apps);

  system_web_app_manager()->set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, pending_app_manager()->install_requests().size());

  // Create another app. The version hasn't changed but the app should still
  // install.
  system_apps[SystemAppType::DISCOVER] = {kAppUrl2};
  system_web_app_manager()->SetSystemApps(system_apps);
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, pending_app_manager()->install_requests().size());

  {
    // Disabling System Web Apps uninstalls without a version change.
    base::test::ScopedFeatureList disable_feature_list;
    disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

    system_web_app_manager()->Start(ui_delegate());

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2u, pending_app_manager()->uninstall_requests().size());
  }

  // Re-enabling System Web Apps installs without a version change.
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, pending_app_manager()->install_requests().size());
}

TEST_F(SystemWebAppManagerTest, UpdateOnVersionChange) {
  system_web_app_manager()->SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps[SystemAppType::SETTINGS] = {kAppUrl1};
  system_web_app_manager()->SetSystemApps(system_apps);

  system_web_app_manager()->set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, pending_app_manager()->install_requests().size());

  // Create another app. The version hasn't changed so the install won't
  // process.
  system_apps[SystemAppType::DISCOVER] = {kAppUrl2};
  system_web_app_manager()->SetSystemApps(system_apps);
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, pending_app_manager()->install_requests().size());

  // Bump the version number, and the install will trigger, and request
  // installation of both apps.
  system_web_app_manager()->set_current_version(base::Version("2.0.0.0"));
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, pending_app_manager()->install_requests().size());

  {
    // Disabling System Web Apps uninstalls even without a version change.
    base::test::ScopedFeatureList disable_feature_list;
    disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

    system_web_app_manager()->Start(ui_delegate());

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2u, pending_app_manager()->uninstall_requests().size());
  }

  // Re-enabling System Web Apps installs even without a version change.
  system_web_app_manager()->Start(ui_delegate());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, pending_app_manager()->install_requests().size());
}

}  // namespace web_app
