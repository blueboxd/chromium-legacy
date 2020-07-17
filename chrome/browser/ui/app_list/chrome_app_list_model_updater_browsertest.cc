// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"

namespace {

constexpr char kOemAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

}  // namespace

class OemAppPositionTest : public chromeos::LoginManagerTest {
 public:
  OemAppPositionTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
  }
  ~OemAppPositionTest() override = default;

  // LoginManagerTest:
  bool SetUpUserDataDirectory() override {
    // Create test user profile directory and copy extensions and preferences
    // from the test data directory to it.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    const std::string& email =
        login_mixin_.users()[0].account_id.GetUserEmail();
    const std::string user_id_hash =
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(email);
    const base::FilePath user_profile_path = user_data_dir.Append(
        chromeos::ProfileHelper::GetUserProfileDir(user_id_hash));
    base::CreateDirectory(user_profile_path);

    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
    src_dir = src_dir.AppendASCII("extensions").AppendASCII("app_list_oem");

    base::CopyFile(src_dir.Append(chrome::kPreferencesFilename),
                   user_profile_path.Append(chrome::kPreferencesFilename));
    base::CopyDirectory(src_dir.AppendASCII("Extensions"), user_profile_path,
                        true);
    return true;
  }

  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(OemAppPositionTest);
};

class AppPositionReorderingTest : public extensions::ExtensionBrowserTest {
 public:
  AppPositionReorderingTest() = default;
  ~AppPositionReorderingTest() override = default;
  AppPositionReorderingTest(const AppPositionReorderingTest& other) = delete;
  AppPositionReorderingTest& operator=(const AppPositionReorderingTest& other) =
      delete;

 protected:
  ash::AppListTestApi app_list_test_api_;
};
// Tests that an Oem app and its folder are created with valid positions after
// sign-in.
IN_PROC_BROWSER_TEST_F(OemAppPositionTest, ValidOemAppPosition) {
  LoginUser(login_mixin_.users()[0].account_id);

  // Ensure apps that are installed upon sign-in are registered with the App
  // Service, resolving any pending messages as a result of running async
  // callbacks.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  ASSERT_TRUE(proxy);
  proxy->FlushMojoCallsForTesting();

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Ensure async callbacks are run.
  base::RunLoop().RunUntilIdle();

  const ChromeAppListItem* oem_app = model_updater->FindItem(kOemAppId);
  ASSERT_TRUE(oem_app);
  EXPECT_TRUE(oem_app->position().IsValid());

  const ChromeAppListItem* oem_folder =
      model_updater->FindItem(ash::kOemFolderId);
  ASSERT_TRUE(oem_folder);
  EXPECT_TRUE(oem_folder->position().IsValid());
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       PRE_ReorderAppPositionInFolder) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();

  // Ensure async callbacks are run.
  base::RunLoop().RunUntilIdle();

  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  const std::string app4_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app4_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  // Create a folder with app1, app2 and app4 in order.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id, app4_id});

  std::vector<std::string> original_id_list{app1_id, app2_id, app4_id};
  ASSERT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), original_id_list);

  // Change an app position in the folder.
  app_list_test_api_.MoveItemToPosition(app1_id, 2);

  std::vector<std::string> reordered_id_list{app2_id, app4_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}

// Tests if the app position changed in a folder persist after the system
// restart.
IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest, ReorderAppPositionInFolder) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();

  // Ensure async callbacks are run.
  base::RunLoop().RunUntilIdle();

  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app4_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  std::string folder_id = app_list_test_api_.GetFolderId(app1_id);
  // Check if the three apps are still in the same folder.
  ASSERT_FALSE(folder_id.empty());
  ASSERT_EQ(app_list_test_api_.GetFolderId(app2_id), folder_id);
  ASSERT_EQ(app_list_test_api_.GetFolderId(app4_id), folder_id);

  std::vector<std::string> reordered_id_list{app2_id, app4_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}
