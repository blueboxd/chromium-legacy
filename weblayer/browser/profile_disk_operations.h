// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_
#define WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace weblayer {

struct ProfileInfo {
  // The profile name supplied by client code. Name can only contain
  // alphanumeric and underscore to be valid. The empty name is valid and
  // indicates the incognito profile.
  std::string name;
  // Path where persistent profile data is stored. This will be empty for the
  // incognito profile with empty name.
  base::FilePath data_path;
  // Path where cache profile data is stored. Depending on the OS, this may
  // be the same as |data_path|; the OS may delete data in this directory.
  base::FilePath cache_path;
};

// |name| must be a valid profile name. Ensures that both data and cache path
// directories are created.
ProfileInfo CreateProfileInfo(const std::string& name);

base::FilePath ComputeBrowserPersisterDataBaseDir(const ProfileInfo& info);
void NukeProfileFromDisk(const ProfileInfo& info);

// Return names of profiles on disk. Invalid profile names are ignored.
std::vector<std::string> ListProfileNames();

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_DISK_OPERATIONS_H_
