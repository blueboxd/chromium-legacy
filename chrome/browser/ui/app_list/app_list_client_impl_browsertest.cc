// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/models/simple_menu_model.h"

// Browser Test for AppListClientImpl.
using AppListClientImplBrowserTest = extensions::PlatformAppBrowserTest;

// Test AppListClient::IsAppOpen for extension apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsExtensionAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_extension_app_id"));

  base::FilePath extension_path = test_data_dir_.AppendASCII("app");
  const extensions::Extension* extension_app = LoadExtension(extension_path);
  ASSERT_NE(nullptr, extension_app);
  EXPECT_FALSE(delegate->IsAppOpen(extension_app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenApplication(AppLaunchParams(
        profile(), extension_app->id(), extensions::LAUNCH_CONTAINER_WINDOW,
        WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(extension_app->id()));
}

// Test AppListClient::IsAppOpen for platform apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsPlatformAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_platform_app_id"));

  const extensions::Extension* app = InstallPlatformApp("minimal");
  EXPECT_FALSE(delegate->IsAppOpen(app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    LaunchPlatformApp(app);
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(app->id()));
}

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, CreateNewWindow) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  ASSERT_TRUE(controller);

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(0U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));

  controller->CreateNewWindow(browser()->profile(), false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile()));

  controller->CreateNewWindow(browser()->profile(), true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));
}

// Test that all the items in the context menu for a hosted app have valid
// labels.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ShowContextMenu) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client);

  // Show the app list to ensure it has loaded a profile.
  client->ShowAppList();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  EXPECT_TRUE(model_updater);

  // Get the webstore hosted app, which is always present.
  ChromeAppListItem* item = model_updater->FindItem(extensions::kWebStoreAppId);
  EXPECT_TRUE(item);

  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  item->GetContextMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
        menu_model = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(menu_model);

  int num_items = menu_model->GetItemCount();
  EXPECT_LT(0, num_items);

  for (int i = 0; i < num_items; i++) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR)
      continue;

    base::string16 label = menu_model->GetLabelAt(i);
    EXPECT_FALSE(label.empty());
  }
}

// Test that OpenSearchResult that dismisses app list runs fine without
// user-after-free.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, OpenSearchResult) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);

  // Associate |client| with the current profile.
  client->UpdateProfile();

  // Show the launcher.
  client->ShowAppList();

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  app_list::SearchController* search_controller = client->search_controller();
  ASSERT_TRUE(search_controller);

  // Any app that opens a window to dismiss app list is good enough for this
  // test.
  const std::string app_title = "chromium";
  const std::string app_result_id =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf/";

  // Search by title and the app must present in the results.
  model_updater->UpdateSearchBox(base::ASCIIToUTF16(app_title),
                                 true /* initiated_by_user */);
  ASSERT_TRUE(search_controller->FindSearchResult(app_result_id));

  // Open the app result.
  client->OpenSearchResult(app_result_id, ui::EF_NONE,
                           ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
                           ash::AppListLaunchType::kAppSearchResult, 0);

  // App list should be dismissed.
  EXPECT_FALSE(client->app_list_target_visibility());

  // Needed to let AppLaunchEventLogger finish its work on worker thread.
  // Otherwise, its |weak_factory_| is released on UI thread and causing
  // the bound WeakPtr to fail sequence check on a worker thread.
  // TODO(crbug.com/965065): Remove after fixing AppLaunchEventLogger.
  content::RunAllTasksUntilIdle();
}

// Test that browser launch time is recorded is recorded in preferences.
// This is important for suggested apps sorting.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest,
                       BrowserLaunchTimeRecorded) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  ASSERT_TRUE(controller);

  Profile* profile = browser()->profile();
  Profile* profile_otr = profile->GetOffTheRecordProfile();

  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile);

  // Starting with just one regular browser.
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile));
  EXPECT_EQ(0U, chrome::GetBrowserCount(profile_otr));

  // First browser launch time should be recorded.
  const base::Time time_recorded1 =
      prefs->GetLastLaunchTime(extension_misc::kChromeAppId);
  EXPECT_NE(base::Time(), time_recorded1);

  // Create an incognito browser so that we can close the regular one without
  // exiting the test.
  controller->CreateNewWindow(profile, true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile_otr));
  // Creating incognito browser should not update the launch time.
  EXPECT_EQ(time_recorded1,
            prefs->GetLastLaunchTime(extension_misc::kChromeAppId));

  // Close the regular browser.
  CloseBrowserSynchronously(chrome::FindBrowserWithProfile(profile));
  EXPECT_EQ(0U, chrome::GetBrowserCount(profile));
  // Recorded the launch time should not update.
  EXPECT_EQ(time_recorded1,
            prefs->GetLastLaunchTime(extension_misc::kChromeAppId));

  // Launch another regular browser.
  const base::Time time_before_launch = base::Time::Now();
  controller->CreateNewWindow(profile, false);
  const base::Time time_after_launch = base::Time::Now();
  EXPECT_EQ(1U, chrome::GetBrowserCount(profile));

  const base::Time time_recorded2 =
      prefs->GetLastLaunchTime(extension_misc::kChromeAppId);
  EXPECT_LE(time_before_launch, time_recorded2);
  EXPECT_GE(time_after_launch, time_recorded2);

  // Creating a second regular browser should not update the launch time.
  controller->CreateNewWindow(profile, false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(profile));
  EXPECT_EQ(time_recorded2,
            prefs->GetLastLaunchTime(extension_misc::kChromeAppId));
}

// Browser Test for AppListClient that observes search result changes.
using AppListClientSearchResultsBrowserTest = extensions::ExtensionBrowserTest;

// Test showing search results, and uninstalling one of them while displayed.
IN_PROC_BROWSER_TEST_F(AppListClientSearchResultsBrowserTest,
                       UninstallSearchResult) {
  // Zero state changes UI behavior. This test case tests the expected UI
  // behavior with zero state being disabled.
  // TODO(jennyz): write new test case for zero state, crbug.com/925195.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      app_list_features::kEnableZeroStateSuggestions);
  ASSERT_FALSE(app_list_features::IsZeroStateSuggestionsEnabled());

  base::FilePath test_extension_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
                            .AppendASCII("platform_apps")
                            .AppendASCII("minimal");

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  // Associate |client| with the current profile.
  client->UpdateProfile();

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  app_list::SearchController* search_controller = client->search_controller();
  ASSERT_TRUE(search_controller);

  // Install the extension.
  const extensions::Extension* extension = InstallExtension(
      test_extension_path, 1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  const std::string title = extension->name();

  // Show the app list first, otherwise we won't have a search box to update.
  client->ShowAppList();

  // Currently the search box is empty, so we have no result.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  // Now a search finds the extension.
  model_updater->UpdateSearchBox(base::ASCIIToUTF16(title),
                                 true /* initiated_by_user */);

  EXPECT_TRUE(search_controller->GetResultByTitleForTest(title));

  // Uninstall the extension.
  UninstallExtension(extension->id());

  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  // We cannot find the extension any more.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  client->DismissView();
}

class AppListClientGuestModeBrowserTest : public InProcessBrowserTest {
 public:
  AppListClientGuestModeBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListClientGuestModeBrowserTest);
};

void AppListClientGuestModeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                  user_manager::kGuestUserName);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                  TestingProfile::kTestUserProfileDir);
  command_line->AppendSwitch(switches::kIncognito);
}

// Test creating the initial app list in guest mode.
IN_PROC_BROWSER_TEST_F(AppListClientGuestModeBrowserTest, Incognito) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client->GetCurrentAppListProfile());

  client->ShowAppList();
  EXPECT_EQ(browser()->profile(), client->GetCurrentAppListProfile());
}

class AppListAppLaunchTest : public extensions::ExtensionBrowserTest {
 protected:
  AppListAppLaunchTest() : extensions::ExtensionBrowserTest() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  ~AppListAppLaunchTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    AppListClientImpl* app_list = AppListClientImpl::GetInstance();
    EXPECT_TRUE(app_list != nullptr);

    // Need to set the profile to get the model updater.
    app_list->UpdateProfile();
    model_updater_ = app_list->GetModelUpdaterForTest();
    EXPECT_TRUE(model_updater_ != nullptr);
  }

  void LaunchChromeAppListItem(const std::string& id) {
    model_updater_->FindItem(id)->PerformActivate(ui::EF_NONE);
  }

  // Captures histograms.
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  AppListModelUpdater* model_updater_;

  DISALLOW_COPY_AND_ASSIGN(AppListAppLaunchTest);
};

IN_PROC_BROWSER_TEST_F(AppListAppLaunchTest,
                       NoDemoModeAppLaunchSourceReported) {
  EXPECT_FALSE(chromeos::DemoSession::IsDeviceInDemoMode());
  LaunchChromeAppListItem(extension_misc::kChromeAppId);

  // Should see 0 apps launched from the Launcher in the histogram when not in
  // Demo mode.
  histogram_tester_->ExpectTotalCount("DemoMode.AppLaunchSource", 0);
}

IN_PROC_BROWSER_TEST_F(AppListAppLaunchTest, DemoModeAppLaunchSourceReported) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  EXPECT_TRUE(chromeos::DemoSession::IsDeviceInDemoMode());

  // Should see 0 apps launched from the Launcher in the histogram at first.
  histogram_tester_->ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  // Launch chrome browser from the Launcher.  The same mechanism
  // (ChromeAppListItem) is used for all types of apps
  // (ARC, extension, etc), so launching just the browser suffices
  // to test all these cases.
  LaunchChromeAppListItem(extension_misc::kChromeAppId);

  // Should see 1 app launched from the Launcher in the histogram.
  histogram_tester_->ExpectUniqueSample(
      "DemoMode.AppLaunchSource",
      chromeos::DemoSession::AppLaunchSource::kAppList, 1);
}
