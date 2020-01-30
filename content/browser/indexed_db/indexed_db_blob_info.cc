// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_blob_info.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

const int64_t IndexedDBBlobInfo::kUnknownSize;

// static
void IndexedDBBlobInfo::ConvertBlobInfo(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  blob_or_file_info->reserve(blob_info.size());
  for (const auto& iter : blob_info) {
    if (!iter.mark_used_callback().is_null())
      iter.mark_used_callback().Run();

    auto info = blink::mojom::IDBBlobInfo::New();
    info->mime_type = iter.type();
    info->size = iter.size();
    if (iter.is_file()) {
      info->file = blink::mojom::IDBFileInfo::New();
      info->file->name = iter.file_name();
      info->file->last_modified = iter.last_modified();
    }
    blob_or_file_info->push_back(std::move(info));
  }
}

IndexedDBBlobInfo::IndexedDBBlobInfo() : is_file_(false), size_(kUnknownSize) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(
    mojo::PendingRemote<blink::mojom::Blob> blob_remote,
    const std::string& uuid,
    const base::string16& type,
    int64_t size)
    : is_file_(false),
      blob_remote_(std::move(blob_remote)),
      uuid_(uuid),
      type_(type),
      size_(size) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(const base::string16& type,
                                     int64_t size,
                                     int64_t blob_number)
    : is_file_(false), type_(type), size_(size), blob_number_(blob_number) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(
    mojo::PendingRemote<blink::mojom::Blob> blob_remote,
    const std::string& uuid,
    const base::string16& file_name,
    const base::string16& type,
    const base::Time& last_modified,
    const int64_t size)
    : is_file_(true),
      blob_remote_(std::move(blob_remote)),
      uuid_(uuid),
      type_(type),
      size_(size),
      file_name_(file_name),
      last_modified_(last_modified) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(int64_t blob_number,
                                     const base::string16& type,
                                     const base::string16& file_name,
                                     const base::Time& last_modified,
                                     const int64_t size)
    : is_file_(true),
      type_(type),
      size_(size),
      file_name_(file_name),
      last_modified_(last_modified),
      blob_number_(blob_number) {}

IndexedDBBlobInfo::IndexedDBBlobInfo(const IndexedDBBlobInfo& other) = default;

IndexedDBBlobInfo::~IndexedDBBlobInfo() = default;

IndexedDBBlobInfo& IndexedDBBlobInfo::operator=(
    const IndexedDBBlobInfo& other) = default;

void IndexedDBBlobInfo::Clone(
    mojo::PendingReceiver<blink::mojom::Blob> receiver) const {
  DCHECK(is_remote_valid());
  blob_remote_->Clone(std::move(receiver));
}

void IndexedDBBlobInfo::set_size(int64_t size) {
  DCHECK_EQ(-1, size_);
  size_ = size;
}

void IndexedDBBlobInfo::set_indexed_db_file_path(
    const base::FilePath& file_path) {
  indexed_db_file_path_ = file_path;
}

void IndexedDBBlobInfo::set_last_modified(const base::Time& time) {
  DCHECK(base::Time().is_null());
  DCHECK(is_file_);
  last_modified_ = time;
}

void IndexedDBBlobInfo::set_blob_number(int64_t blob_number) {
  DCHECK_EQ(DatabaseMetaDataKey::kInvalidBlobNumber, blob_number_);
  blob_number_ = blob_number;
}

void IndexedDBBlobInfo::set_mark_used_callback(
    base::RepeatingClosure mark_used_callback) {
  DCHECK(!mark_used_callback_);
  mark_used_callback_ = std::move(mark_used_callback);
}

void IndexedDBBlobInfo::set_release_callback(
    base::RepeatingClosure release_callback) {
  DCHECK(!release_callback_);
  release_callback_ = std::move(release_callback);
}

}  // namespace content
