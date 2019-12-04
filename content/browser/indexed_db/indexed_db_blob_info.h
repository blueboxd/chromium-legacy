// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_INFO_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

class CONTENT_EXPORT IndexedDBBlobInfo {
 public:
  static void ConvertBlobInfo(
      const std::vector<IndexedDBBlobInfo>& blob_info,
      std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info);

  IndexedDBBlobInfo();
  // These two are used for Blobs.
  IndexedDBBlobInfo(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
                    const std::string& uuid,
                    const base::string16& type,
                    int64_t size);
  IndexedDBBlobInfo(const base::string16& type, int64_t size, int64_t key);
  // These two are used for Files.
  IndexedDBBlobInfo(mojo::PendingRemote<blink::mojom::Blob> blob_remote,
                    const std::string& uuid,
                    const base::FilePath& file_path,
                    const base::string16& file_name,
                    const base::string16& type);
  IndexedDBBlobInfo(int64_t key,
                    const base::string16& type,
                    const base::string16& file_name);

  IndexedDBBlobInfo(const IndexedDBBlobInfo& other);
  ~IndexedDBBlobInfo();
  IndexedDBBlobInfo& operator=(const IndexedDBBlobInfo& other);

  bool is_file() const { return is_file_; }
  bool is_remote_valid() const { return blob_remote_.is_bound(); }
  const std::string& uuid() const {
    DCHECK(is_remote_valid());
    return uuid_;
  }
  void Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) const;
  mojo::SharedRemote<blink::mojom::Blob> remote() const { return blob_remote_; }
  const base::string16& type() const { return type_; }
  int64_t size() const { return size_; }
  const base::string16& file_name() const { return file_name_; }
  int64_t key() const { return key_; }
  const base::FilePath& file_path() const { return file_path_; }
  const base::Time& last_modified() const { return last_modified_; }
  const base::RepeatingClosure& mark_used_callback() const {
    return mark_used_callback_;
  }
  const base::RepeatingClosure& release_callback() const {
    return release_callback_;
  }

  void set_size(int64_t size);
  void set_file_path(const base::FilePath& file_path);
  void set_last_modified(const base::Time& time);
  void set_key(int64_t key);
  void set_mark_used_callback(base::RepeatingClosure mark_used_callback);
  void set_release_callback(base::RepeatingClosure release_callback);

 private:
  bool is_file_;

  // Always for Blob; sometimes for File.
  mojo::SharedRemote<blink::mojom::Blob> blob_remote_;
  // If blob_remote_ is true, this is the blob's uuid.
  std::string uuid_;
  // Mime type.
  base::string16 type_;
  // -1 if unknown for File.
  int64_t size_;
  // Only for File.
  base::string16 file_name_;
  // Only for File.
  base::FilePath file_path_;
  // Only for File; valid only if size is.
  base::Time last_modified_;

  // Valid only when this comes out of the database.
  int64_t key_;
  base::RepeatingClosure mark_used_callback_;
  base::RepeatingClosure release_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_INFO_H_
