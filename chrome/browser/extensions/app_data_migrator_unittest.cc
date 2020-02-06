// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/app_data_migrator.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/mock_blob_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
std::unique_ptr<TestingProfile> GetTestingProfile() {
  TestingProfile::Builder profile_builder;
  return profile_builder.Build();
}
}  // namespace

namespace extensions {

class AppDataMigratorTest : public testing::Test {
 public:
  AppDataMigratorTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    profile_ = GetTestingProfile();
    registry_ = ExtensionRegistry::Get(profile_.get());
    migrator_ = std::unique_ptr<AppDataMigrator>(
        new AppDataMigrator(profile_.get(), registry_));

    default_partition_ =
        content::BrowserContext::GetDefaultStoragePartition(profile_.get());

    idb_context_ = default_partition_->GetIndexedDBContext();

    default_fs_context_ = default_partition_->GetFileSystemContext();

    blob_storage_context_ = std::make_unique<storage::BlobStorageContext>();
  }

  void TearDown() override {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AppDataMigrator> migrator_;
  content::StoragePartition* default_partition_;
  ExtensionRegistry* registry_;
  storage::FileSystemContext* default_fs_context_;
  content::IndexedDBContext* idb_context_;
  std::unique_ptr<storage::BlobStorageContext> blob_storage_context_;
};

scoped_refptr<const Extension> GetTestExtension(bool platform_app) {
  scoped_refptr<const Extension> app;
  if (platform_app) {
    app = ExtensionBuilder()
              .SetManifest(
                  DictionaryBuilder()
                      .Set("name", "test app")
                      .Set("version", "1")
                      .Set("app", DictionaryBuilder()
                                      .Set("background",
                                           DictionaryBuilder()
                                               .Set("scripts",
                                                    ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                               .Build())
                                      .Build())
                      .Set("permissions",
                           ListBuilder().Append("unlimitedStorage").Build())
                      .Build())
              .Build();
  } else {
    app = ExtensionBuilder()
              .SetManifest(
                  DictionaryBuilder()
                      .Set("name", "test app")
                      .Set("version", "1")
                      .Set("app", DictionaryBuilder()
                                      .Set("launch",
                                           DictionaryBuilder()
                                               .Set("local_path", "index.html")
                                               .Build())
                                      .Build())
                      .Set("permissions",
                           ListBuilder().Append("unlimitedStorage").Build())
                      .Build())
              .Build();
  }
  return app;
}

void OpenFileSystem(storage::FileSystemContext* fs_context,
                    GURL extension_url,
                    storage::FileSystemType type) {
  base::RunLoop run_loop;
  fs_context->OpenFileSystem(
      extension_url, type, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindLambdaForTesting([&](const GURL& root, const std::string& name,
                                     base::File::Error result) {
        EXPECT_EQ(result, base::File::FILE_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void CreateAndWriteFile(storage::FileSystemContext* fs_context,
                        const storage::FileSystemURL& url,
                        std::unique_ptr<storage::BlobDataHandle> blob) {
  {
    base::RunLoop run_loop;
    fs_context->operation_runner()->CreateFile(
        url, false, base::BindLambdaForTesting([&](base::File::Error result) {
          EXPECT_EQ(result, base::File::FILE_OK);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    fs_context->operation_runner()->Write(
        url, std::move(blob), 0,
        base::BindLambdaForTesting(
            [&](base::File::Error result, int64_t bytes, bool complete) {
              if (complete)
                run_loop.Quit();
              EXPECT_EQ(result, base::File::FILE_OK);
            }));
    run_loop.Run();
  }
}

void GenerateTestFiles(storage::BlobStorageContext* blob_storage_context,
                       const Extension* ext,
                       storage::FileSystemContext* fs_context,
                       Profile* profile) {
  profile->GetExtensionSpecialStoragePolicy()->GrantRightsForExtension(ext);

  base::FilePath path(FILE_PATH_LITERAL("test.txt"));
  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(ext->id());

  OpenFileSystem(fs_context, extension_url, storage::kFileSystemTypeTemporary);
  OpenFileSystem(fs_context, extension_url, storage::kFileSystemTypePersistent);

  storage::FileSystemURL fs_temp_url = fs_context->CreateCrackedFileSystemURL(
      extension_url, storage::kFileSystemTypeTemporary, path);

  storage::FileSystemURL fs_persistent_url =
      fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypePersistent, path);

  storage::ScopedTextBlob blob1(blob_storage_context, "blob-id:success1",
                                "Hello, world!\n");

  CreateAndWriteFile(fs_context, fs_temp_url, blob1.GetBlobDataHandle());
  CreateAndWriteFile(fs_context, fs_persistent_url, blob1.GetBlobDataHandle());
}

void VerifyFileContents(storage::FileSystemContext* new_fs_context,
                        const storage::FileSystemURL& url) {
  base::RunLoop run_loop;
  new_fs_context->operation_runner()->OpenFile(
      url, base::File::FLAG_READ | base::File::FLAG_OPEN,
      base::BindLambdaForTesting(
          [&](base::File file, base::OnceClosure on_close_callback) {
            ASSERT_EQ(14, file.GetLength());
            std::unique_ptr<char[]> buffer(new char[15]);

            file.Read(0, buffer.get(), 14);
            buffer.get()[14] = 0;

            std::string expected = "Hello, world!\n";
            std::string actual = buffer.get();
            EXPECT_EQ(expected, actual);

            file.Close();
            if (!on_close_callback.is_null())
              std::move(on_close_callback).Run();
            run_loop.Quit();
          }));
  run_loop.Run();
}

void VerifyTestFilesMigrated(content::StoragePartition* new_partition,
                             const Extension* new_ext) {
  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(new_ext->id());
  storage::FileSystemContext* new_fs_context =
      new_partition->GetFileSystemContext();

  OpenFileSystem(new_fs_context, extension_url,
                 storage::kFileSystemTypeTemporary);
  OpenFileSystem(new_fs_context, extension_url,
                 storage::kFileSystemTypePersistent);

  base::FilePath path(FILE_PATH_LITERAL("test.txt"));

  storage::FileSystemURL fs_temp_url =
      new_fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypeTemporary, path);
  storage::FileSystemURL fs_persistent_url =
      new_fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypePersistent, path);

  VerifyFileContents(new_fs_context, fs_temp_url);
  VerifyFileContents(new_fs_context, fs_persistent_url);
}

TEST_F(AppDataMigratorTest, ShouldMigrate) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  EXPECT_TRUE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, ShouldNotMigratePlatformApp) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(true);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  EXPECT_FALSE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, ShouldNotMigrateLegacyApp) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(false);

  EXPECT_FALSE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, NoOpMigration) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  // Nothing to migrate. Basically this should just not cause an error
  migrator_->DoMigrationAndReply(old_ext.get(), new_ext.get(),
                                 base::DoNothing());
}

TEST_F(AppDataMigratorTest, FileSystemMigration) {
  // When writing files, this touches the quota manager, which then
  // kicks off extra tasks to write to the quota database that fail
  // when the test is over.  Because this test is not about quota,
  // disable the quota manager database for both partitions.
  default_partition_->GetQuotaManager()->DisableDatabaseForTesting();

  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  GenerateTestFiles(blob_storage_context_.get(), old_ext.get(),
                    default_fs_context_, profile_.get());

  base::RunLoop run_loop;
  migrator_->DoMigrationAndReply(old_ext.get(), new_ext.get(),
                                 run_loop.QuitClosure());
  run_loop.Run();

  registry_->AddEnabled(new_ext);
  content::StoragePartition* new_partition =
      util::GetStoragePartitionForExtensionId(new_ext->id(), profile_.get());
  new_partition->GetQuotaManager()->DisableDatabaseForTesting();
  ASSERT_NE(new_partition->GetPath(), default_partition_->GetPath());

  VerifyTestFilesMigrated(new_partition, new_ext.get());
}

}  // namespace extensions
