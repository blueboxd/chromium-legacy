// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"

namespace file_manager {
namespace test {

FolderInMyFiles::~FolderInMyFiles() = default;

void FolderInMyFiles::Add(const std::vector<base::FilePath>& files) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  for (const auto& file : files) {
    files_.push_back(folder_.Append(file.BaseName()));
    base::CopyFile(file, files_.back());
  }
}

FolderInMyFiles::FolderInMyFiles(Profile* profile) {
  const base::FilePath root = util::GetMyFilesFolderForProfile(profile);
  VolumeManager::Get(profile)->RegisterDownloadsDirectoryForTesting(root);

  base::ScopedAllowBlockingForTesting allow_blocking;
  constexpr base::FilePath::CharType kPrefix[] = FILE_PATH_LITERAL("a_folder");
  CHECK(CreateTemporaryDirInDir(root, kPrefix, &folder_));
}

void AddDefaultComponentExtensionsOnMainThread(Profile* profile) {
  CHECK(profile);

  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->component_loader()->AddDefaultComponentExtensions(false);

  // The File Manager component extension should have been added for loading
  // into the user profile, but not into the sign-in profile.
  CHECK(extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->component_loader()
            ->Exists(kFileManagerAppId));
  CHECK(!extensions::ExtensionSystem::Get(
             chromeos::ProfileHelper::GetSigninProfile())
             ->extension_service()
             ->component_loader()
             ->Exists(kFileManagerAppId));
}

namespace {
// Helper to exit a RunLoop the next time OnVolumeMounted() is invoked.
class VolumeWaiter : public VolumeManagerObserver {
 public:
  VolumeWaiter(Profile* profile, const base::Closure& on_mount)
      : profile_(profile), on_mount_(on_mount) {
    VolumeManager::Get(profile_)->AddObserver(this);
  }

  ~VolumeWaiter() override {
    VolumeManager::Get(profile_)->RemoveObserver(this);
  }

  void OnVolumeMounted(chromeos::MountError error_code,
                       const Volume& volume) override {
    on_mount_.Run();
  }

 private:
  Profile* profile_;
  base::Closure on_mount_;
};
}  // namespace

scoped_refptr<const extensions::Extension> InstallTestingChromeApp(
    Profile* profile,
    const char* test_path_ascii) {
  base::ScopedAllowBlockingForTesting allow_io;
  content::WindowedNotificationObserver handler_ready(
      extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::NotificationService::AllSources());
  extensions::ChromeTestExtensionLoader loader(profile);

  base::FilePath path;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII(test_path_ascii);

  auto extension = loader.LoadExtension(path);
  CHECK(extension);
  handler_ready.Wait();
  return extension;
}

base::WeakPtr<file_manager::Volume> InstallFileSystemProviderChromeApp(
    Profile* profile) {
  static constexpr char kFileSystemProviderFilesystemId[] =
      "test-image-provider-fs";
  base::RunLoop run_loop;
  file_manager::test::VolumeWaiter waiter(profile, run_loop.QuitClosure());
  auto extension = InstallTestingChromeApp(
      profile, "extensions/api_test/file_browser/image_provider");
  run_loop.Run();

  auto* volume_manager = file_manager::VolumeManager::Get(profile);
  CHECK(volume_manager);
  base::WeakPtr<file_manager::Volume> volume;
  for (auto& v : volume_manager->GetVolumeList()) {
    if (v->file_system_id() == kFileSystemProviderFilesystemId)
      volume = v;
  }

  CHECK(volume);
  return volume;
}

}  // namespace test
}  // namespace file_manager
