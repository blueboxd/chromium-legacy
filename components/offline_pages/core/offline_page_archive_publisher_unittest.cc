// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_archive_publisher.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/stub_system_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int64_t kDownloadId = 42LL;
}  // namespace

namespace offline_pages {

class OfflinePageArchivePublisherTest
    : public testing::Test,
      public base::SupportsWeakPtr<OfflinePageArchivePublisherTest> {
 public:
  OfflinePageArchivePublisherTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        weak_ptr_factory_(this) {}
  ~OfflinePageArchivePublisherTest() override {}

  SavePageCallback save_page_callback;

  void SetUp() override;
  void PumpLoop();

  OfflinePageItemGenerator* page_generator() { return &page_generator_; }

  const base::FilePath& temporary_dir_path() {
    return temporary_dir_.GetPath();
  }
  const base::FilePath& private_archive_dir_path() {
    return private_archive_dir_.GetPath();
  }
  const base::FilePath& public_archive_dir_path() {
    return public_archive_dir_.GetPath();
  }
  const PublishArchiveResult& publish_archive_result() {
    return publish_archive_result_;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }
  base::WeakPtr<OfflinePageArchivePublisherTest> get_weak_ptr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void PublishArchiveDone(SavePageCallback save_page_callback,
                          const OfflinePageItem& offline_page,
                          PublishArchiveResult archive_result);

 private:
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir private_archive_dir_;
  base::ScopedTempDir public_archive_dir_;
  OfflinePageItemGenerator page_generator_;
  PublishArchiveResult publish_archive_result_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::WeakPtrFactory<OfflinePageArchivePublisherTest> weak_ptr_factory_;
};

void OfflinePageArchivePublisherTest::SetUp() {
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(private_archive_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(public_archive_dir_.CreateUniqueTempDir());
}

void OfflinePageArchivePublisherTest::PublishArchiveDone(
    SavePageCallback save_page_callback,
    const OfflinePageItem& offline_page,
    PublishArchiveResult archive_result) {
  publish_archive_result_ = archive_result;
}

void OfflinePageArchivePublisherTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(OfflinePageArchivePublisherTest, PublishArchive) {
  auto archive_manager = std::make_unique<ArchiveManager>(
      temporary_dir_path(), private_archive_dir_path(),
      public_archive_dir_path(), task_runner());
  auto download_manager =
      std::make_unique<StubSystemDownloadManager>(kDownloadId, true);

  OfflinePageArchivePublisher archiver(archive_manager.get(),
                                       download_manager.get());

  // Put an offline page into the private dir, adjust the FilePath.
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  OfflinePageItem offline_page = page_generator()->CreateItemWithTempFile();
  base::FilePath old_file_path = offline_page.file_path;
  base::FilePath new_file_path =
      public_archive_dir_path().Append(offline_page.file_path.BaseName());

  archiver.PublishArchive(
      offline_page, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&OfflinePageArchivePublisherTest::PublishArchiveDone,
                     get_weak_ptr(), std::move(save_page_callback)));
  PumpLoop();

  EXPECT_EQ(SavePageResult::SUCCESS, publish_archive_result().move_result);
  EXPECT_EQ(kDownloadId, publish_archive_result().download_id);
  // Check there is a file in the new location.
  EXPECT_TRUE(public_archive_dir_path().IsParent(
      publish_archive_result().new_file_path));
  EXPECT_TRUE(base::PathExists(publish_archive_result().new_file_path));
  // Check there is no longer a file in the old location.
  EXPECT_FALSE(base::PathExists(old_file_path));
}

// TODO(petewil): Add test cases for move failed, and adding to ADM failed.

}  // namespace offline_pages
