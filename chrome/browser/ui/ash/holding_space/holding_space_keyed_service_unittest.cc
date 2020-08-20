// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Utility class that registers an external file system mount point, and grants
// file manager app access permission for the mount point.
class ScopedExternalMountPoint {
 public:
  ScopedExternalMountPoint(Profile* profile, const std::string& name)
      : name_(name) {
    if (!temp_dir_.CreateUniqueTempDir())
      return;

    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        name_, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), temp_dir_.GetPath());
    file_manager::util::GetFileSystemContextForExtensionId(
        profile, file_manager::kFileManagerAppId)
        ->external_backend()
        ->GrantFileAccessToExtension(file_manager::kFileManagerAppId,
                                     base::FilePath(name_));
  }

  ~ScopedExternalMountPoint() {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
  }

  bool IsValid() const { return temp_dir_.IsValid(); }

  const base::FilePath& GetRootPath() const { return temp_dir_.GetPath(); }

  const std::string& name() const { return name_; }

 private:
  base::ScopedTempDir temp_dir_;
  std::string name_;
};

}  // namespace

class HoldingSpaceKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  HoldingSpaceKeyedServiceTest()
      : fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
    scoped_feature_list_.InitAndEnableFeature(features::kTemporaryHoldingSpace);
  }
  HoldingSpaceKeyedServiceTest(const HoldingSpaceKeyedServiceTest& other) =
      delete;
  HoldingSpaceKeyedServiceTest& operator=(
      const HoldingSpaceKeyedServiceTest& other) = delete;
  ~HoldingSpaceKeyedServiceTest() override = default;

  TestingProfile* CreateProfile() override {
    const std::string kPrimaryProfileName = "primary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kPrimaryProfileName));

    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    GetSessionControllerClient()->AddUserSession(kPrimaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);

    return profile_manager()->CreateTestingProfile(kPrimaryProfileName);
  }

  TestingProfile* CreateSecondaryProfile() {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    return profile_manager()->CreateTestingProfile(
        kSecondaryProfileName,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::ASCIIToUTF16("Test profile"), 1 /*avatar_id*/,
        std::string() /*supervised_user_id*/,
        TestingProfile::TestingFactories());
  }

  void ActivateSecondaryProfile() {
    const std::string kSecondaryProfileName = "secondary_profile";
    const AccountId account_id(AccountId::FromUserEmail(kSecondaryProfileName));
    GetSessionControllerClient()->AddUserSession(kSecondaryProfileName);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client();
  }

  // Creates a file under path |mount_point_path|/|relative_path| with the
  // provided content.
  // Returns the created file's file path, or an empty path on failure.
  base::FilePath CreateFile(const base::FilePath& mount_point_path,
                            const base::FilePath& relative_path,
                            const std::string& content) {
    const base::FilePath path = mount_point_path.Append(relative_path);
    if (!base::CreateDirectory(path.DirName()))
      return base::FilePath();
    if (!base::WriteFile(path, content))
      return base::FilePath();
    return path;
  }

  // Resolves a file system URL in the file manager's file system context, and
  // returns the file's virtual path relative to the mount point root.
  // Returns an empty file if the URL cannot be resolved to a file. For example,
  // if it's not well formed, or the file manager app cannot access it.
  base::FilePath GetVirtualPathFromUrl(
      const GURL& url,
      const std::string& expected_mount_point) {
    storage::FileSystemContext* fs_context =
        file_manager::util::GetFileSystemContextForExtensionId(
            GetProfile(), file_manager::kFileManagerAppId);
    storage::FileSystemURL fs_url = fs_context->CrackURL(url);

    base::RunLoop run_loop;
    base::FilePath result;
    base::FilePath* result_ptr = &result;
    fs_context->ResolveURL(
        fs_url,
        base::BindLambdaForTesting(
            [&run_loop, &expected_mount_point, &result_ptr](
                base::File::Error result, const storage::FileSystemInfo& info,
                const base::FilePath& file_path,
                storage::FileSystemContext::ResolvedEntryType type) {
              EXPECT_EQ(base::File::Error::FILE_OK, result);
              EXPECT_EQ(storage::FileSystemContext::RESOLVED_ENTRY_FILE, type);
              if (expected_mount_point == info.name) {
                *result_ptr = file_path;
              } else {
                ADD_FAILURE() << "Mount point name '" << info.name
                              << "' does not match expected '"
                              << expected_mount_point << "'";
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

 private:
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests adding a screenshot item. Verifies that adding a screenshot creates a
// holding space item with a file system URL that can be accessed by the file
// manager app.
TEST_F(HoldingSpaceKeyedServiceTest, AddScreenshotItem) {
  // Create a test downloads mount point.
  ScopedExternalMountPoint downloads_mount(
      GetProfile(),
      file_manager::util::GetDownloadsMountPointName(GetProfile()));
  ASSERT_TRUE(downloads_mount.IsValid());

  // Verify that the holding space model gets set even if the holding space
  // keyed service is not explicitly created.
  HoldingSpaceModel* const initial_model =
      HoldingSpaceController::Get()->model();
  EXPECT_TRUE(initial_model);

  HoldingSpaceKeyedService* const holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  const base::FilePath item_1_virtual_path("Screenshot 1.png");
  // Create a fake screenshot file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_1_full_path =
      CreateFile(downloads_mount.GetRootPath(), item_1_virtual_path, "red");
  ASSERT_FALSE(item_1_full_path.empty());

  holding_space_service->AddScreenshot(
      item_1_full_path, CreateSolidColorImage(64, 64, SK_ColorRED));

  const base::FilePath item_2_virtual_path =
      base::FilePath("Alt/Screenshot 2.png");
  // Create a fake screenshot file on the local file system - later parts of the
  // test will try to resolve the file's file system URL, which fails if the
  // file does not exist.
  const base::FilePath item_2_full_path =
      CreateFile(downloads_mount.GetRootPath(), item_2_virtual_path, "blue");
  ASSERT_FALSE(item_2_full_path.empty());
  holding_space_service->AddScreenshot(
      item_2_full_path, CreateSolidColorImage(64, 64, SK_ColorBLUE));

  EXPECT_EQ(initial_model, HoldingSpaceController::Get()->model());
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            holding_space_service->model_for_testing());

  HoldingSpaceModel* const model = HoldingSpaceController::Get()->model();
  ASSERT_EQ(2u, model->items().size());

  const HoldingSpaceItem* item_1 = model->items()[0].get();
  EXPECT_EQ(item_1_full_path, item_1->file_path());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(*CreateSolidColorImage(64, 64, SK_ColorRED).bitmap(),
                           *item_1->image().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(
      item_1_virtual_path,
      GetVirtualPathFromUrl(item_1->file_system_url(), downloads_mount.name()));
  EXPECT_EQ(base::ASCIIToUTF16("Screenshot 1.png"), item_1->text());

  const HoldingSpaceItem* item_2 = model->items()[1].get();
  EXPECT_EQ(item_2_full_path, item_2->file_path());
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *CreateSolidColorImage(64, 64, SK_ColorBLUE).bitmap(),
      *item_2->image().bitmap()));
  // Verify the item file system URL resolves to the correct file in the file
  // manager's context.
  EXPECT_EQ(
      item_2_virtual_path,
      GetVirtualPathFromUrl(item_2->file_system_url(), downloads_mount.name()));
  EXPECT_EQ(base::ASCIIToUTF16("Screenshot 2.png"), item_2->text());
}

TEST_F(HoldingSpaceKeyedServiceTest, SecondaryUserProfile) {
  HoldingSpaceKeyedService* const primary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  TestingProfile* const second_profile = CreateSecondaryProfile();
  HoldingSpaceKeyedService* const secondary_holding_space_service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
          second_profile);

  // Just creating a secondary profile should not change the active model.
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            primary_holding_space_service->model_for_testing());

  // Switching the active user should change the active model (multi user
  // support)
  ActivateSecondaryProfile();
  EXPECT_EQ(HoldingSpaceController::Get()->model(),
            secondary_holding_space_service->model_for_testing());
}

}  // namespace ash
