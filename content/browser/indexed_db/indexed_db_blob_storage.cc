// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_blob_storage.h"

#include "base/callback.h"

namespace content {

BlobChangeRecord::BlobChangeRecord(const std::string& object_store_data_key)
    : object_store_data_key_(object_store_data_key) {}

BlobChangeRecord::~BlobChangeRecord() = default;

void BlobChangeRecord::SetBlobInfo(std::vector<IndexedDBBlobInfo>* blob_info) {
  blob_info_.clear();
  if (blob_info)
    blob_info_.swap(*blob_info);
}

std::unique_ptr<BlobChangeRecord> BlobChangeRecord::Clone() const {
  std::unique_ptr<BlobChangeRecord> record(
      new BlobChangeRecord(object_store_data_key_));
  record->blob_info_ = blob_info_;

  return record;
}

}  // namespace content
