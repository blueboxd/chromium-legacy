// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/profile_disk_operations.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

class ProfileDiskOperationsTest : public testing::Test {
 public:
  void SetUp() override {
    CHECK(data_temp_dir_.CreateUniqueTempDir());
    base::PathService::Override(DIR_USER_DATA, data_temp_dir_.GetPath());
#if defined(OS_POSIX)
    CHECK(cache_temp_dir_.CreateUniqueTempDir());
    base::PathService::Override(base::DIR_CACHE, cache_temp_dir_.GetPath());
#endif
  }

 protected:
  base::ScopedTempDir data_temp_dir_;
#if defined(OS_POSIX)
  base::ScopedTempDir cache_temp_dir_;
#endif
};

TEST_F(ProfileDiskOperationsTest, IsProfileNameValid) {
  EXPECT_TRUE(internal::IsProfileNameValid("foo"));
  EXPECT_TRUE(internal::IsProfileNameValid("123"));
  EXPECT_TRUE(internal::IsProfileNameValid(std::string()));

  EXPECT_FALSE(internal::IsProfileNameValid("foo.bar"));
  EXPECT_FALSE(internal::IsProfileNameValid("foo~"));
  EXPECT_FALSE(internal::IsProfileNameValid("foo-"));
}

TEST_F(ProfileDiskOperationsTest, CheckDirnameAndExtractName) {
  EXPECT_EQ(std::string("foo123"),
            internal::CheckDirNameAndExtractName("foo123"));
  EXPECT_EQ(std::string("foo"), internal::CheckDirNameAndExtractName("foo.1"));
  EXPECT_EQ(std::string("foo"), internal::CheckDirNameAndExtractName("foo.2"));
  EXPECT_EQ(std::string("foo"),
            internal::CheckDirNameAndExtractName("foo.123"));

  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName("foo."));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName("foo~"));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName("foo~.1"));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName("foo.bar"));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName("foo.1.2"));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName(std::string()));
  EXPECT_EQ(std::string(), internal::CheckDirNameAndExtractName(".1"));
}

TEST_F(ProfileDiskOperationsTest, BasicListProfileNames) {
  std::vector<std::string> names{"foo", "bar", "baz"};
  for (const auto& name : names) {
    ProfileInfo info = CreateProfileInfo(name);
    EXPECT_FALSE(info.data_path.empty());
    EXPECT_FALSE(info.cache_path.empty());
  }
  std::vector<std::string> listed_names = ListProfileNames();
  EXPECT_EQ(names.size(), listed_names.size());
  for (const auto& name : names) {
    auto itr = std::find(listed_names.begin(), listed_names.end(), name);
    EXPECT_TRUE(itr != listed_names.end());
  }
}

TEST_F(ProfileDiskOperationsTest, MarkProfileAsDeleted) {
  std::vector<std::string> names{"foo", "bar", "baz"};
  std::vector<ProfileInfo> infos;
  for (const auto& name : names) {
    ProfileInfo info = CreateProfileInfo(name);
    infos.push_back(info);
    EXPECT_FALSE(info.data_path.empty());
    EXPECT_FALSE(info.cache_path.empty());
  }
  for (const auto& info : infos) {
    MarkProfileAsDeleted(info);
    EXPECT_TRUE(internal::IsProfileMarkedForDeletion(
        info.data_path.BaseName().MaybeAsASCII()));
  }
  std::vector<std::string> listed_names = ListProfileNames();
  EXPECT_TRUE(listed_names.empty());
}

TEST_F(ProfileDiskOperationsTest, ReuseProfileName) {
  constexpr int kRepeat = 3;
  for (int i = 0; i < kRepeat; ++i) {
    ProfileInfo info = CreateProfileInfo("foo");
    MarkProfileAsDeleted(info);
    EXPECT_TRUE(internal::IsProfileMarkedForDeletion(
        info.data_path.BaseName().MaybeAsASCII()));

    std::string expected_base_name("foo");
    if (i != 0) {
      expected_base_name += ".";
      expected_base_name += base::NumberToString(i);
    }
    EXPECT_EQ(expected_base_name, info.data_path.BaseName().MaybeAsASCII());
    EXPECT_EQ(expected_base_name, info.cache_path.BaseName().MaybeAsASCII());

    std::vector<std::string> listed_names = ListProfileNames();
    EXPECT_TRUE(listed_names.empty());
  }
}

TEST_F(ProfileDiskOperationsTest, NukeProfile) {
  std::vector<ProfileInfo> deleted_infos;
  constexpr int kRepeat = 3;
  for (int i = 0; i < kRepeat; ++i) {
    ProfileInfo info = CreateProfileInfo("foo");
    MarkProfileAsDeleted(info);
    deleted_infos.push_back(info);
  }

  {
    ProfileInfo info = CreateProfileInfo("bar");
    MarkProfileAsDeleted(info);
    deleted_infos.push_back(info);
  }

  {
    ProfileInfo info = CreateProfileInfo("baz");
    MarkProfileAsDeleted(info);
    deleted_infos.push_back(info);
  }

  ProfileInfo kept_info = CreateProfileInfo("kept");

  for (auto& info : deleted_infos) {
    EXPECT_TRUE(base::PathExists(info.data_path));
    EXPECT_TRUE(base::PathExists(info.cache_path));
  }
  EXPECT_TRUE(base::PathExists(kept_info.data_path));
  EXPECT_TRUE(base::PathExists(kept_info.cache_path));

  NukeProfilesMarkedForDeletion();

  for (auto& info : deleted_infos) {
    EXPECT_FALSE(base::PathExists(info.data_path));
    EXPECT_FALSE(base::PathExists(info.cache_path));
  }
  EXPECT_TRUE(base::PathExists(kept_info.data_path));
  EXPECT_TRUE(base::PathExists(kept_info.cache_path));

  ProfileInfo info = CreateProfileInfo("bar");
  EXPECT_EQ(std::string("bar"), info.data_path.BaseName().MaybeAsASCII());
  EXPECT_EQ(std::string("bar"), info.cache_path.BaseName().MaybeAsASCII());
}

}  // namespace weblayer
