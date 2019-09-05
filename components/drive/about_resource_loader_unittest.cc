// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/about_resource_loader.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/event_logger.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

struct DestroyHelper {
  template <typename T>
  void operator()(T* object) const {
    if (object) {
      object->Destroy();
    }
  }
};

}  // namespace

class AboutResourceLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::TestMockTimeTaskRunner::Type::kBoundToThread);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_util::RegisterDrivePrefs(pref_service_->registry());

    logger_ = std::make_unique<EventLogger>();

    drive_service_ = std::make_unique<FakeDriveService>();
    ASSERT_TRUE(test_util::SetUpTestEntries(drive_service_.get()));

    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
    scheduler_ = std::make_unique<JobScheduler>(
        pref_service_.get(), logger_.get(), drive_service_.get(),
        network::TestNetworkConnectionTracker::GetInstance(),
        task_runner_.get(), mojo::NullRemote());
    metadata_storage_.reset(
        new ResourceMetadataStorage(temp_dir_.GetPath(), task_runner_.get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(), temp_dir_.GetPath(),
                               task_runner_.get(),
                               nullptr /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    metadata_.reset(new ResourceMetadata(metadata_storage_.get(), cache_.get(),
                                         task_runner_.get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    about_resource_loader_ = std::make_unique<AboutResourceLoader>(
        scheduler_.get(), task_runner_->GetMockTickClock());
  }

  void TearDown() override {
    // We need to manually reset the objects that implement the Destroy idiom,
    // that deletes the object on the |task_runner_|. This is simpler than
    // introducing custom deleters that capture the |task_runner_| and
    // invoke RunUntilIdle().
    metadata_.reset();
    cache_.reset();
    metadata_storage_.reset();
    task_runner_->RunUntilIdle();
  }

  // Adds a new file to the root directory of the service.
  std::unique_ptr<google_apis::FileResource> AddNewFile(
      const std::string& title) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_FILE_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    drive_service_->AddNewFile(
        "text/plain", "content text", drive_service_->GetRootResourceId(),
        title,
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    task_runner_->RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_CREATED, error);
    return entry;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<EventLogger> logger_;
  std::unique_ptr<FakeDriveService> drive_service_;
  std::unique_ptr<JobScheduler> scheduler_;
  std::unique_ptr<ResourceMetadataStorage, DestroyHelper> metadata_storage_;
  std::unique_ptr<FileCache, DestroyHelper> cache_;
  std::unique_ptr<ResourceMetadata, DestroyHelper> metadata_;
  std::unique_ptr<AboutResourceLoader> about_resource_loader_;
};

TEST_F(AboutResourceLoaderTest, AboutResourceLoader) {
  google_apis::DriveApiErrorCode error[6] = {};
  std::unique_ptr<google_apis::AboutResource> about[6];

  // No resource is cached at the beginning.
  ASSERT_FALSE(about_resource_loader_->cached_about_resource());

  // Since no resource is cached, this "Get" should trigger the update.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 0, about + 0));
  // Since there is one in-flight update, the next "Get" just wait for it.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 1, about + 1));

  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[0]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[1]);
  const int64_t first_changestamp = about[0]->largest_change_id();
  EXPECT_EQ(first_changestamp, about[1]->largest_change_id());
  ASSERT_TRUE(about_resource_loader_->cached_about_resource());
  EXPECT_EQ(
      first_changestamp,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  // Increment changestamp by 1.
  AddNewFile("temp");
  // Explicitly calling UpdateAboutResource will start another API call.
  about_resource_loader_->UpdateAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 2, about + 2));
  // It again waits for the in-flight UpdateAboutResoure call, even though this
  // time there is a cached result.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 3, about + 3));

  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[2]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[3]);
  EXPECT_EQ(first_changestamp + 1, about[2]->largest_change_id());
  EXPECT_EQ(first_changestamp + 1, about[3]->largest_change_id());
  EXPECT_EQ(
      first_changestamp + 1,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  // Increment changestamp by 1.
  AddNewFile("temp2");
  // Now no UpdateAboutResource task is running. Returns the cached result.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 4, about + 4));
  // Explicitly calling UpdateAboutResource will start another API call.
  about_resource_loader_->UpdateAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 5, about + 5));

  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT, error[4]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[5]);
  EXPECT_EQ(first_changestamp + 1, about[4]->largest_change_id());
  EXPECT_EQ(first_changestamp + 2, about[5]->largest_change_id());
  EXPECT_EQ(
      first_changestamp + 2,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  EXPECT_EQ(3, drive_service_->about_resource_load_count());
}

TEST_F(AboutResourceLoaderTest, EvictCache) {
  google_apis::DriveApiErrorCode error;
  std::unique_ptr<google_apis::AboutResource> about;

  // No resource is cached at the beginning.
  ASSERT_FALSE(about_resource_loader_->cached_about_resource());

  // Since no resource is cached, this "Get" should trigger the update.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(&error, &about));

  task_runner_->RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

  // Should have a cached resource.
  ASSERT_TRUE(about_resource_loader_->cached_about_resource());

  // Advance the timer should evict the cache.
  task_runner_->FastForwardUntilNoTasksRemain();

  // Cache should be evicted.
  ASSERT_FALSE(about_resource_loader_->cached_about_resource());
}

}  // namespace internal
}  // namespace drive
