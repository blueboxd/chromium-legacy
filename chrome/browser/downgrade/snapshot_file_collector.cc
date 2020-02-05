// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_file_collector.h"

#include <utility>

namespace downgrade {

SnapshotItemDetails::SnapshotItemDetails(base::FilePath path,
                                         ItemType item_type)
    : path(std::move(path)), is_directory(item_type == ItemType::kDirectory) {}

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems(uint16_t version) {
  // TODO (ydago): move the logic of the component files to the component
  // themselves.
  return std::vector<SnapshotItemDetails>{
      // General files to snapshot
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Last Browser")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Local State")),
                          SnapshotItemDetails::ItemType::kDirectory),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Avatars")),
                          SnapshotItemDetails::ItemType::kDirectory)};
}

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems(uint16_t version) {
  // TODO (ydago): move the logic of the component files to the component
  // themselves.
  return std::vector<SnapshotItemDetails>{
      // General Profile files
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Preferences")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("Secure Preferences")),
          SnapshotItemDetails::ItemType::kFile),
      // History files
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("History")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Favicons")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("TopSites")),
                          SnapshotItemDetails::ItemType::kFile),
      // Bookmarks
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Bookmarks")),
                          SnapshotItemDetails::ItemType::kFile),
      // Tab Restore and sessions
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Current Tabs")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Current Session")),
                          SnapshotItemDetails::ItemType::kFile),
      // Sign-in state
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("Google Profile.ico")),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("Google Profile Picture.png")),
          SnapshotItemDetails::ItemType::kFile),
      // Password / Autofill
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("Affiliation Database")),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Login Data")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("Login Data For Account")),
          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Web Data")),
                          SnapshotItemDetails::ItemType::kFile),
      SnapshotItemDetails(
          base::FilePath(FILE_PATH_LITERAL("AutofillStrikeDatabase")),
          SnapshotItemDetails::ItemType::kFile),
      // Cookies
      SnapshotItemDetails(base::FilePath(FILE_PATH_LITERAL("Cookies")),
                          SnapshotItemDetails::ItemType::kFile)};
}

}  // namespace downgrade
