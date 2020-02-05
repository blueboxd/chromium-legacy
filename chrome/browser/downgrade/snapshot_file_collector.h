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

  SnapshotItemDetails(base::FilePath path, ItemType type);
  ~SnapshotItemDetails() = default;
  const base::FilePath path;
  const bool is_directory;
};

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems(uint16_t version);

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems(uint16_t version);

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_SNAPSHOT_FILE_COLLECTOR_H_
