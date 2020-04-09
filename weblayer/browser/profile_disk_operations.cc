// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_disk_operations.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "weblayer/common/weblayer_paths.h"

namespace weblayer {

namespace {

bool IsNameValid(const std::string& name) {
  for (char c : name) {
    if (!(base::IsAsciiDigit(c) || base::IsAsciiAlpha(c) || c == '_'))
      return false;
  }
  return true;
}

// Return the data path directory to profiles.
base::FilePath GetProfileRootDataDir() {
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  return path.AppendASCII("profiles");
}

}  // namespace

ProfileInfo CreateProfileInfo(const std::string& name) {
  CHECK(IsNameValid(name));
  if (name.empty())
    return {name, base::FilePath(), base::FilePath()};

  base::FilePath data_path = GetProfileRootDataDir().AppendASCII(name.c_str());
  if (!base::PathExists(data_path))
    base::CreateDirectory(data_path);
  base::FilePath cache_path = data_path;
#if defined(OS_POSIX)
  CHECK(base::PathService::Get(base::DIR_CACHE, &cache_path));
  cache_path = cache_path.AppendASCII("profiles").AppendASCII(name.c_str());
  if (!base::PathExists(cache_path))
    base::CreateDirectory(cache_path);
#endif
  return {name, data_path, cache_path};
}

base::FilePath ComputeBrowserPersisterDataBaseDir(const ProfileInfo& info) {
  base::FilePath base_path;
  if (info.data_path.empty()) {
    CHECK(base::PathService::Get(DIR_USER_DATA, &base_path));
    base_path = base_path.AppendASCII("Incognito Restore Data");
  } else {
    base_path = info.data_path.AppendASCII("Restore Data");
  }
  return base_path;
}

void NukeProfileFromDisk(const ProfileInfo& info) {
  if (info.name.empty()) {
    // Incognito. Just delete session data.
    base::DeleteFileRecursively(ComputeBrowserPersisterDataBaseDir(info));
    return;
  }

  base::DeleteFileRecursively(info.data_path);
#if defined(OS_POSIX)
  base::DeleteFileRecursively(info.cache_path);
#endif
}

std::vector<std::string> ListProfileNames() {
  base::FilePath root_dir = GetProfileRootDataDir();
  std::vector<std::string> profile_names;
  base::FileEnumerator enumerator(root_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string name = enumerator.GetInfo().GetName().MaybeAsASCII();
    if (IsNameValid(name))
      profile_names.push_back(name);
  }
  return profile_names;
}

}  // namespace weblayer
