// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace app_list {

// This contains almost end-to-end tests for the launcher search backend. It is
// set up to simulate user input by calls to the AppListClient, and observe the
// results that would be displayed via the AppListModelUpdater. This class is
// also intended as in-code documentation for how to create future app list
// search integration tests.
class AppListSearchBrowserTest : public chromeos::LoginManagerTest {
 public:
  using ResultType = ash::AppListSearchResultType;

  AppListSearchBrowserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
  }
  ~AppListSearchBrowserTest() override = default;

  AppListSearchBrowserTest(const AppListSearchBrowserTest&) = delete;
  AppListSearchBrowserTest& operator=(const AppListSearchBrowserTest&) = delete;

  //---------------
  // Search helpers
  //---------------

  // The AppListClient is in charge of communication from ash to chrome, so can
  // be used to mimic UI actions. Examples include starting a search, launching
  // a result, or possibly activating a particular view.
  AppListClientImpl* GetClient() {
    auto* client = ::test::GetAppListClient();
    CHECK(client);
    return client;
  }

  void StartSearch(const std::string& query) {
    GetClient()->StartSearch(base::ASCIIToUTF16(query));
  }

  void SearchAndWaitForProviders(const std::string& query,
                                 const std::set<ResultType> providers) {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    std::set<ResultType> finished_providers;
    const SearchController::ResultsChangedCallback callback =
        base::BindLambdaForTesting([&](ResultType provider) {
          finished_providers.insert(provider);

          // Quit the run loop if all |providers| are finished.
          for (const auto& type : providers) {
            if (finished_providers.find(type) == finished_providers.end())
              return;
          }
          quit_closure.Run();
        });

    // The ordering of this logic is important. The results changed callback
    // must be set before the call to StartSearch, to avoid a race between a
    // provider returning and the callback being set, which could lead to the
    // run loop timing out.
    GetClient()->search_controller()->set_results_changed_callback_for_test(
        std::move(callback));
    GetClient()->StartSearch(base::ASCIIToUTF16(query));
    run_loop.Run();
    // Once the run loop is finished, we have to remove the callback because the
    // referenced variables are about to go out of scope.
    GetClient()->search_controller()->set_results_changed_callback_for_test(
        base::DoNothing());
  }

  // Returns a search result for the given |id|, or nullptr if no matching
  // search result exists.
  std::vector<ChromeSearchResult*> PublishedResults() {
    return GetClient()
        ->GetModelUpdaterForTest()
        ->GetPublishedSearchResultsForTest();
  }

  ChromeSearchResult* FindResult(const std::string& id) {
    for (auto* result : PublishedResults()) {
      if (result->id() == id)
        return result;
    }
    return nullptr;
  }

  //----------------
  // Session helpers
  //----------------

  Profile* GetProfile() {
    auto* profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    return profile;
  }

  base::FilePath GetProfileDir() { return profile_dir_; }

  void DoLogin() { LoginUser(login_mixin_.users()[0].account_id); }

  //-----
  // Misc
  //-----

  // LoginManagerTest:
  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    const std::string& email =
        login_mixin_.users()[0].account_id.GetUserEmail();
    const std::string user_id_hash =
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(email);
    profile_dir_ = user_data_dir.Append(
        chromeos::ProfileHelper::GetUserProfileDir(user_id_hash));
    base::CreateDirectory(profile_dir_);

    return true;
  }

 protected:
  base::FilePath profile_dir_;

  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};
};

// Test fixture for OS settings search. This subclass exists because changing a
// feature flag has to be done in the constructor. Otherwise, it could use
// AppListSearchBrowserTest directly.
class OsSettingsSearchBrowserTest : public AppListSearchBrowserTest {
 public:
  OsSettingsSearchBrowserTest() : AppListSearchBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {app_list_features::kLauncherSettingsSearch,
         chromeos::features::kNewOsSettingsSearch},
        {});
  }
  ~OsSettingsSearchBrowserTest() override = default;

  OsSettingsSearchBrowserTest(const OsSettingsSearchBrowserTest&) = delete;
  OsSettingsSearchBrowserTest& operator=(const OsSettingsSearchBrowserTest&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Simply tests that neither zero-state nor query-based search cause a crash.
IN_PROC_BROWSER_TEST_F(AppListSearchBrowserTest, SearchDoesntCrash) {
  DoLogin();
  // This won't catch everything, because not all providers run on all queries,
  // and so we can't wait for all providers to finish. Instead, we wait on one
  // app and one non-app provider. Note file search (kLauncher) is generally the
  // slowest provider.
  SearchAndWaitForProviders("",
                            {ResultType::kInstalledApp, ResultType::kLauncher});
  SearchAndWaitForProviders("some query",
                            {ResultType::kInstalledApp, ResultType::kLauncher});
}

// Test that searching for "wifi" correctly returns a settings result for wifi.
IN_PROC_BROWSER_TEST_F(OsSettingsSearchBrowserTest, AppListSearchForSettings) {
  DoLogin();
  SearchAndWaitForProviders("wifi", {ResultType::kOsSettings});

  auto* result = FindResult("os-settings://networks?type=WiFi");
  ASSERT_TRUE(result);
  EXPECT_EQ(base::UTF16ToASCII(result->accessible_name()),
            "Wi-Fi networks, Network, Settings");
}

}  // namespace app_list
