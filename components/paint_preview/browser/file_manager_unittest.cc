// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/file_manager.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

TEST(FileManagerTest, TestStats) {
  base::Time now = base::Time::Now();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  GURL url("https://www.chromium.org");
  base::FilePath directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(url, &directory));
  EXPECT_FALSE(directory.empty());

  base::Time created_time;
  EXPECT_TRUE(manager.GetCreatedTime(url, &created_time));

  base::TouchFile(directory, now - base::TimeDelta::FromSeconds(1),
                  now - base::TimeDelta::FromSeconds(1));
  base::Time accessed_time;
  EXPECT_TRUE(manager.GetLastAccessedTime(url, &accessed_time));
  base::FilePath proto_path = directory.AppendASCII("paint_preview.pb");
  base::File file(proto_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const size_t kSize = 50;
  std::string zeros(kSize, '0');
  file.Write(0, zeros.data(), zeros.size());
  file.Close();
  base::TouchFile(directory, now + base::TimeDelta::FromSeconds(1),
                  now + base::TimeDelta::FromSeconds(1));
  base::Time later_accessed_time;
  EXPECT_TRUE(manager.GetLastAccessedTime(url, &later_accessed_time));
  EXPECT_GT(later_accessed_time, accessed_time);

  EXPECT_GE(manager.GetSizeOfArtifactsFor(url), kSize);
}

TEST(FileManagerTest, TestCreateOrGetDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  GURL url("https://www.chromium.org");
  base::FilePath new_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(url, &new_directory));
  EXPECT_FALSE(new_directory.empty());
  EXPECT_TRUE(base::PathExists(new_directory));
  base::FilePath old_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(url, &old_directory));
  EXPECT_FALSE(old_directory.empty());
  EXPECT_EQ(old_directory, new_directory);
  EXPECT_TRUE(base::PathExists(old_directory));
}

TEST(FileManagerTest, TestDeleteArtifactsFor) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  GURL cr_url("https://www.chromium.org");
  base::FilePath cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(cr_url, &cr_directory));
  EXPECT_FALSE(cr_directory.empty());
  EXPECT_TRUE(base::PathExists(cr_directory));
  GURL w3_url("https://www.w3.org");
  base::FilePath w3_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(w3_url, &w3_directory));
  EXPECT_FALSE(w3_directory.empty());
  EXPECT_TRUE(base::PathExists(w3_directory));
  manager.DeleteArtifactsFor(std::vector<GURL>({cr_url}));
  EXPECT_FALSE(base::PathExists(cr_directory));
  EXPECT_TRUE(base::PathExists(w3_directory));
  base::FilePath new_cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(cr_url, &new_cr_directory));
  EXPECT_FALSE(new_cr_directory.empty());
  EXPECT_TRUE(base::PathExists(new_cr_directory));
  EXPECT_EQ(cr_directory, new_cr_directory);
  manager.DeleteArtifactsFor(std::vector<GURL>({cr_url, w3_url}));
  EXPECT_FALSE(base::PathExists(new_cr_directory));
  EXPECT_FALSE(base::PathExists(w3_directory));
}

TEST(FileManagerTest, TestDeleteAll) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  GURL cr_url("https://www.chromium.org");
  base::FilePath cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(cr_url, &cr_directory));
  EXPECT_FALSE(cr_directory.empty());
  EXPECT_TRUE(base::PathExists(cr_directory));
  GURL w3_url("https://www.w3.org");
  base::FilePath w3_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(w3_url, &w3_directory));
  EXPECT_FALSE(w3_directory.empty());
  EXPECT_TRUE(base::PathExists(w3_directory));
  manager.DeleteAll();
  EXPECT_FALSE(base::PathExists(cr_directory));
  EXPECT_FALSE(base::PathExists(w3_directory));
}

TEST(FileManagerTest, TestDeleteAllOlderThan) {
  base::Time now = base::Time::Now();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FileManager manager(temp_dir.GetPath());
  GURL cr_url("https://www.chromium.org");
  base::FilePath cr_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(cr_url, &cr_directory));
  EXPECT_FALSE(cr_directory.empty());
  EXPECT_TRUE(base::PathExists(cr_directory));
  base::TouchFile(cr_directory, now - base::TimeDelta::FromSeconds(1),
                  now - base::TimeDelta::FromSeconds(1));

  GURL w3_url("https://www.w3.org");
  base::FilePath w3_directory;
  EXPECT_TRUE(manager.CreateOrGetDirectoryFor(w3_url, &w3_directory));
  EXPECT_FALSE(w3_directory.empty());
  EXPECT_TRUE(base::PathExists(w3_directory));
  base::TouchFile(w3_directory, now + base::TimeDelta::FromSeconds(1),
                  now + base::TimeDelta::FromSeconds(1));
  manager.DeleteAllOlderThan(now);
  EXPECT_FALSE(base::PathExists(cr_directory));
  EXPECT_TRUE(base::PathExists(w3_directory));
}

}  // namespace paint_preview
