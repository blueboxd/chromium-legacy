// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_util.h"

namespace arc {

bool AppendRelativePathForRemovableMedia(const base::FilePath& cros_path,
                                         base::FilePath* android_path) {
  std::vector<base::FilePath::StringType> parent_components;
  base::FilePath(kCrosRemovableMediaDir).GetComponents(&parent_components);
  std::vector<base::FilePath::StringType> child_components;
  cros_path.GetComponents(&child_components);
  auto child_itr = child_components.begin();
  for (const auto& parent_component : parent_components) {
    if (child_itr == child_components.end() || parent_component != *child_itr) {
      LOG(WARNING) << "|cros_path| is not under kCrosRemovableMediaDir.";
      return false;
    }
    ++child_itr;
  }
  if (child_itr == child_components.end()) {
    LOG(WARNING) << "The CrOS path doesn't have a component for device label.";
    return false;
  }
  // The device label (e.g. "UNTITLED" for /media/removable/UNTITLED/foo.jpg)
  // should be converted to removable_UNTITLED, since the prefix "removable_"
  // is appended to paths for removable media in Android.
  *android_path = android_path->Append(kRemovableMediaLabelPrefix + *child_itr);
  ++child_itr;
  for (; child_itr != child_components.end(); ++child_itr) {
    *android_path = android_path->Append(*child_itr);
  }
  return true;
}

base::FilePath GetAndroidPath(const base::FilePath& cros_path,
                              const base::FilePath& cros_dir,
                              const base::FilePath& android_dir) {
  base::FilePath android_path(android_dir);
  if (cros_dir.value() == kCrosRemovableMediaDir) {
    if (!AppendRelativePathForRemovableMedia(cros_path, &android_path))
      return base::FilePath();
  } else {
    cros_dir.AppendRelativePath(cros_path, &android_path);
  }
  return android_path;
}

}  // namespace arc
