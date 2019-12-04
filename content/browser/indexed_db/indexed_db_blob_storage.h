// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_STORAGE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_STORAGE_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace storage {
class FileWriterDelegate;
}  // namespace storage

namespace content {

// This file contains all of the classes & types used to store blobs in
// IndexedDB. Currently it is messy because this is mid-refactor, but it will be
// cleaned up over time.

enum class BlobWriteResult {
  // There was an error writing the blobs.
  kFailure,
  // The blobs were written, and phase two should be scheduled asynchronously.
  // The returned status will be ignored.
  kRunPhaseTwoAsync,
  // The blobs were written, and phase two should be run now. The returned
  // status will be correctly propagated.
  kRunPhaseTwoAndReturnResult,
};

// This callback is used to signify that writing blobs is complete. The
// BlobWriteResult signifies if the operation succeeded or not, and the returned
// status is used to handle errors in the next part of the transcation commit
// lifecycle. Note: The returned status can only be used when the result is
// |kRunPhaseTwoAndReturnResult|.
using BlobWriteCallback = base::OnceCallback<leveldb::Status(BlobWriteResult)>;

// This object represents a change in the database involving adding or removing
// blobs. if blob_info() is empty, then blobs are to be deleted, and if
// blob_info() is populated, then blobs are two be written (and also possibly
// deleted if there were already blobs).
class BlobChangeRecord {
 public:
  BlobChangeRecord(const std::string& object_store_data_key);
  ~BlobChangeRecord();

  const std::string& object_store_data_key() const {
    return object_store_data_key_;
  }
  void SetBlobInfo(std::vector<IndexedDBBlobInfo>* blob_info);
  std::vector<IndexedDBBlobInfo>& mutable_blob_info() { return blob_info_; }
  const std::vector<IndexedDBBlobInfo>& blob_info() const { return blob_info_; }
  std::unique_ptr<BlobChangeRecord> Clone() const;

 private:
  std::string object_store_data_key_;
  std::vector<IndexedDBBlobInfo> blob_info_;
  DISALLOW_COPY_AND_ASSIGN(BlobChangeRecord);
};

// This holds a BlobEntryKey and the encoded IndexedDBBlobInfo vector stored
// under that key.
typedef std::vector<std::pair<BlobEntryKey, std::string>>
    BlobEntryKeyValuePairVec;

// This is a blob that is going to to be written to a file.
class CONTENT_EXPORT WriteDescriptor {
 public:
  WriteDescriptor(mojo::SharedRemote<blink::mojom::Blob> blob,
                  int64_t key,
                  int64_t size,
                  base::Time last_modified);
  WriteDescriptor(const base::FilePath& path,
                  int64_t key,
                  int64_t size,
                  base::Time last_modified);
  WriteDescriptor(const WriteDescriptor& other);
  ~WriteDescriptor();
  WriteDescriptor& operator=(const WriteDescriptor& other);

  bool is_file() const { return is_file_; }
  mojo::SharedRemote<blink::mojom::Blob> blob() const {
    DCHECK(!is_file_);
    return blob_;
  }
  const base::FilePath& file_path() const {
    DCHECK(is_file_);
    return file_path_;
  }
  int64_t key() const { return key_; }
  int64_t size() const { return size_; }
  base::Time last_modified() const { return last_modified_; }

 private:
  bool is_file_;
  mojo::SharedRemote<blink::mojom::Blob> blob_;
  base::FilePath file_path_;
  int64_t key_;
  int64_t size_;
  base::Time last_modified_;
};

typedef std::vector<WriteDescriptor> WriteDescriptorVec;

// This class facilitates writing multiple blobs to files.
class ChainedBlobWriter : public base::RefCountedThreadSafe<ChainedBlobWriter> {
 public:
  // Called on the IO thread.
  virtual void set_delegate(
      std::unique_ptr<storage::FileWriterDelegate> delegate) = 0;

  // Called on the IDB task runner.
  virtual void ReportWriteCompletion(bool succeeded, int64_t bytes_written) = 0;

  // Called on the IDB task runner.
  virtual void Abort() = 0;

  // Whether to flush to the file system when writing or not.
  // Called on the IO thread.
  virtual storage::FlushPolicy GetFlushPolicy() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<ChainedBlobWriter>;
  virtual ~ChainedBlobWriter() {}
};

// This holds a BlobEntryKey and the encoded IndexedDBBlobInfo vector stored
// under that key.
typedef std::vector<std::pair<BlobEntryKey, std::string>>
    BlobEntryKeyValuePairVec;

// Reports that the recovery and/or active journals have been processed, and
// blob files have been deleted.
using BlobFilesCleanedCallback = base::RepeatingClosure;

// Reports that there are (or are not) active blobs.
using ReportOutstandingBlobsCallback =
    base::RepeatingCallback<void(/*outstanding_blobs=*/bool)>;

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BLOB_STORAGE_H_
