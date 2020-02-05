// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace downgrade {

TEST(UserDataDowngradeTests, GetInvalidSnapshots) {
  base::ScopedTempDir snapshot_dir;
  ASSERT_TRUE(snapshot_dir.CreateUniqueTempDir());
  base::FilePath snapshot_path = snapshot_dir.GetPath();
  for (const std::string& name : std::vector<std::string>{"10", "11", "30"}) {
    ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII(name)));
    base::File(
        snapshot_path.AppendASCII(name).Append(kDowngradeLastVersionFile),
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("20")));
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("Snapshot 20")));
  base::File(snapshot_path.AppendASCII("Snapshot 20")
                 .Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("Something")));
  auto snapshots = GetInvalidSnapshots(snapshot_path);
  base::flat_set<base::FilePath> expected{
      snapshot_path.AppendASCII("Snapshot 20"), snapshot_path.AppendASCII("20"),
      snapshot_path.AppendASCII("Something")};
  EXPECT_EQ(base::flat_set<base::FilePath>(snapshots), expected);
}

TEST(UserDataDowngradeTests, GetAvailableSnapshots) {
  base::ScopedTempDir snapshot_dir;
  ASSERT_TRUE(snapshot_dir.CreateUniqueTempDir());

  for (const std::string& name :
       std::vector<std::string>{"8", "10.0.0", "11.0.11.123", "30.0.1.1"}) {
    ASSERT_TRUE(
        base::CreateDirectory(snapshot_dir.GetPath().AppendASCII(name)));
    base::File(snapshot_dir.GetPath().AppendASCII(name).Append(
                   kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("20.0.2")));
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("Snapshot 20")));
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("Something")));
  base::File(snapshot_dir.GetPath().AppendASCII("9"),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  auto snapshots = GetAvailableSnapshots(snapshot_dir.GetPath());
  base::flat_set<base::Version> expected{
      base::Version("8"), base::Version("10.0.0"), base::Version("11.0.11.123"),
      base::Version("30.0.1.1")};
  EXPECT_EQ(snapshots, expected);
}

TEST(UserDataDowngradeTests, GetSnapshotToRestore) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  const base::FilePath snapshot_dir =
      user_data_dir.GetPath().Append(kSnapshotsDir);
  ASSERT_TRUE(base::CreateDirectory(snapshot_dir.AppendASCII("9")));
  for (const std::string& name :
       std::vector<std::string>{"10.0.0", "11.3.2", "20.0.0", "30.0.0"}) {
    ASSERT_TRUE(base::CreateDirectory(snapshot_dir.AppendASCII(name)));
    base::File(snapshot_dir.AppendASCII(name).Append(kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  EXPECT_EQ(GetSnapshotToRestore(base::Version("9"), user_data_dir.GetPath()),
            base::nullopt);
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("10.1.0"), user_data_dir.GetPath()),
      base::Version("10.0.0"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("15.5.3"), user_data_dir.GetPath()),
      base::Version("11.3.2"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("30.0.0"), user_data_dir.GetPath()),
      base::Version("30.0.0"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("31.0.0"), user_data_dir.GetPath()),
      base::Version("30.0.0"));
}

}  // namespace downgrade
