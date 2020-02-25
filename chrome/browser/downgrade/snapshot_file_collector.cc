// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_file_collector.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/chrome_constants.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/history/core/browser/history_constants.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/sessions/core/session_constants.h"
#include "components/webdata/common/webdata_constants.h"

#if defined(OS_WIN)
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"
#endif

namespace downgrade {

SnapshotItemDetails::SnapshotItemDetails(base::FilePath path,
                                         ItemType item_type)
    : path(std::move(path)), is_directory(item_type == ItemType::kDirectory) {}

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems(uint16_t version) {
  std::vector<SnapshotItemDetails> user_data_items{
      SnapshotItemDetails(base::FilePath(chrome::kLocalStateFilename),
                          SnapshotItemDetails::ItemType::kDirectory),
      SnapshotItemDetails(base::FilePath(profiles::kHighResAvatarFolderName),
                          SnapshotItemDetails::ItemType::kDirectory)};
#if defined(OS_WIN)
  user_data_items.emplace_back(
      SnapshotItemDetails(base::FilePath(web_app::kLastBrowserFile),
                          SnapshotItemDetails::ItemType::kFile));
#endif  // defined(OS_WIN)
  return user_data_items;
}

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems(uint16_t version) {
  std::vector<SnapshotItemDetails> profile_items{
      // General Profile files
      SnapshotItemDetails(base::FilePath(chrome::kPreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(chrome::kSecurePreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile),
      // History files
      SnapshotItemDetails(base::FilePath(history::kHistoryFilename),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(history::kFaviconsFilename),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(history::kTopSitesFilename),
                          SnapshotItemDetails::ItemType::kFile),
      // Bookmarks
      SnapshotItemDetails(base::FilePath(bookmarks::kBookmarksFileName),
                          SnapshotItemDetails::ItemType::kFile),
      // Tab Restore and sessions
      SnapshotItemDetails(base::FilePath(sessions::kCurrentTabSessionFileName),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(sessions::kCurrentSessionFileName),
                          SnapshotItemDetails::ItemType::kFile),
      // Sign-in state
      SnapshotItemDetails(base::FilePath(profiles::kGAIAPictureFileName),
                          SnapshotItemDetails::ItemType::kFile),
      // Password / Autofill
      SnapshotItemDetails(
          base::FilePath(password_manager::kAffiliationDatabaseFileName),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForProfileFileName),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForAccountFileName),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(kWebDataFilename),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(autofill::kStrikeDatabaseFileName),
                          SnapshotItemDetails::ItemType::kFile),
      // Cookies
      SnapshotItemDetails(base::FilePath(chrome::kCookieFilename),
                          SnapshotItemDetails::ItemType::kFile)};

#if defined(OS_WIN)
  // Sign-in state
  profile_items.emplace_back(
      SnapshotItemDetails(base::FilePath(profiles::kProfileIconFileName),
                          SnapshotItemDetails::ItemType::kFile));
#endif  // defined(OS_WIN)
  return profile_items;
}

}  // namespace downgrade
