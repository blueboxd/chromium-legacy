// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/roaming_profile_directory_deleter_win.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "chrome/common/chrome_paths_internal.h"

namespace {

enum class RoamingUserDataDirectoryDeletionResult {
  kError = 0,         // An unexpected error occurred during processing.
  kDoesNotExist = 1,  // Roaming User Data dir does not exist.
  kSucceeded = 2,     // Roaming User Data dir was deleted.
  kFailed = 3,        // Roaming User Data dir could not be deleted.
  kNotInAppData = 4,  // Roaming User Data dir is not within the AppData dir.
  kMaxValue = kNotInAppData
};

// Deletes the user's roaming User Data directory if it is empty, along with any
// of its parent directories leading up to the user's roaming AppData directory.
RoamingUserDataDirectoryDeletionResult DeleteRoamingUserDataDirectoryImpl() {
  base::FilePath to_delete;
  if (!chrome::GetDefaultRoamingUserDataDirectory(&to_delete))
    return RoamingUserDataDirectoryDeletionResult::kError;
  if (!base::DirectoryExists(to_delete))
    return RoamingUserDataDirectoryDeletionResult::kDoesNotExist;

  base::FilePath app_data;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_data))
    return RoamingUserDataDirectoryDeletionResult::kError;
  if (!app_data.IsParent(to_delete))
    return RoamingUserDataDirectoryDeletionResult::kNotInAppData;

  // Consider a failure to delete the deepest dir a failure. This likely means
  // that the directory is not empty. Deleting intermediate dirs between it and
  // the user's AppData directory is best-effort.
  auto result = RoamingUserDataDirectoryDeletionResult::kFailed;
  do {
    // A non-recursive directory delete will fail if the directory is not empty.
    if (!base::DeleteFile(to_delete, /*recursive=*/false))
      return result;
    result = RoamingUserDataDirectoryDeletionResult::kSucceeded;
    to_delete = to_delete.DirName();
  } while (app_data.IsParent(to_delete));
  return result;
}

void DeleteRoamingUserDataDirectory() {
  auto result = DeleteRoamingUserDataDirectoryImpl();
  base::UmaHistogramEnumeration("Sync.DeleteRoamingUserDataDirectory", result);
}

}  // namespace

void DeleteRoamingUserDataDirectoryLater() {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&DeleteRoamingUserDataDirectory));
}
