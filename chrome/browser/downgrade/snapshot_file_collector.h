// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_
#define CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_

#include <vector>

#include "base/files/file_path.h"

namespace downgrade {

struct SnapshotItemDetails {
  enum class ItemType { kFile, kDirectory };

  SnapshotItemDetails(base::FilePath path, ItemType type, int data_types);
  ~SnapshotItemDetails() = default;
  const base::FilePath path;
  const bool is_directory;

  // Bitfield from ChromeBrowsingDataRemoverDelegate::DataType representing
  // the data types affected by this item.
  int data_types;
};

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems();

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems();

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_
