// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_history {

class MediaHistoryStoreUnitTest : public testing::Test {
 public:
  MediaHistoryStoreUnitTest() = default;
  void SetUp() override {
    // Set up the profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath());

    // Set up the media history store.
    scoped_refptr<base::UpdateableSequencedTaskRunner> task_runner =
        base::CreateUpdateableSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::WithBaseSyncPrimitives()});
    media_history_store_ = std::make_unique<MediaHistoryStore>(
        profile_builder.Build().get(), task_runner);

    // Sleep the thread to allow the media history store to asynchronously
    // create the database and tables before proceeding with the tests and
    // tearing down the temporary directory.
    content::RunAllTasksUntilIdle();

    // Set up the local DB connection used for assertions.
    base::FilePath db_file =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Media History"));
    EXPECT_TRUE(db_.Open(db_file));
  }

 protected:
  sql::Database& GetDB() { return db_; }
  content::BrowserTaskEnvironment task_environment_;

 private:
  sql::Database db_;
  std::unique_ptr<MediaHistoryStore> media_history_store_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(MediaHistoryStoreUnitTest, CreateDatabaseTables) {
  ASSERT_TRUE(GetDB().DoesTableExist("mediaEngagement"));
  ASSERT_TRUE(GetDB().DoesTableExist("origin"));
  ASSERT_TRUE(GetDB().DoesTableExist("playback"));
}

}  // namespace media_history
