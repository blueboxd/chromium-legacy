// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
#define STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace storage {
class BlobDataBuilder;
class BlobStorageContext;
class FileSystemContext;

// Ref counted blob item. This class owns the backing data of the blob item. The
// backing data is immutable, and cannot change after creation. The purpose of
// this class is to allow the resource to stick around in the snapshot even
// after the resource was swapped in the blob (either to disk or to memory) by
// the BlobStorageContext.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobDataItem
    : public base::RefCounted<BlobDataItem> {
 public:
  enum class Type {
    kBytes,
    kBytesDescription,
    kFile,
    kFileFilesystem,
    kReadableDataHandle,
  };

  // The DataHandle class is used to persist resources that are needed for
  // reading this BlobDataItem. This object will stay around while any reads are
  // pending. If all blobs with this item are deleted or the item is swapped for
  // a different backend version (mem-to-disk or the reverse), then the item
  // will be destructed after all pending reads are complete.
  //
  // A DataHandle can also be "readable" if it overrides the size and reading
  // virtual methods.  If the DataHandle provides this kind of encapsulated
  // implementation then it can be added to the item using the
  // AppendReadableDataHandle() method.
  class COMPONENT_EXPORT(STORAGE_BROWSER) DataHandle
      : public base::RefCounted<DataHandle> {
   public:
    // Must return the main blob data size.  Returns 0 by default.
    virtual uint64_t GetSize() const;

    // Reads the given data range into the given buffer.  If the read is
    // completed synchronously then the number of bytes read should be returned
    // directly.  If the read must be performed asynchronously then this
    // method must return net::ERR_IO_PENDING and invoke the callback with the
    // result at a later time.  The default implementation returns
    // net::ERR_FILE_NOT_FOUND.
    virtual int Read(scoped_refptr<net::IOBuffer> dst_buffer,
                     uint64_t src_offset,
                     int bytes_to_read,
                     base::OnceCallback<void(int)> callback);

    // Must return the side data size.  If there is no side data, then 0 should
    // be returned.  Returns 0 by default.
    virtual uint64_t GetSideDataSize() const;

    // Reads the entire side data into the given buffer.  The buffer must be
    // large enough to contain the entire side data.  If the read is completed
    // synchronously then the number of bytes read should be returned directly.
    // If the read must be performed asynchronously then this method must
    // return net::ERR_IO_PENDING and invoke the callback with the result at a
    // later time.  The default implementation returns net::ERR_FILE_NOT_FOUND.
    virtual int ReadSideData(scoped_refptr<net::IOBuffer> dst_buffer,
                             base::OnceCallback<void(int)> callback);

    // Print a description of the readable DataHandle for debugging.
    virtual void PrintTo(::std::ostream* os) const;

    // Return the histogram label to use when calling RecordBytesRead().  If
    // nullptr is returned then nothing will be recorded.  Returns nullptr by
    // default.
    virtual const char* BytesReadHistogramLabel() const;

   protected:
    virtual ~DataHandle();

   private:
    friend class base::RefCounted<DataHandle>;
  };

  static scoped_refptr<BlobDataItem> CreateBytes(base::span<const char> bytes);
  static scoped_refptr<BlobDataItem> CreateBytesDescription(size_t length);
  static scoped_refptr<BlobDataItem> CreateFile(base::FilePath path);
  static scoped_refptr<BlobDataItem> CreateFile(
      base::FilePath path,
      uint64_t offset,
      uint64_t length,
      base::Time expected_modification_time = base::Time(),
      scoped_refptr<DataHandle> data_handle = nullptr);
  static scoped_refptr<BlobDataItem> CreateFutureFile(uint64_t offset,
                                                      uint64_t length,
                                                      uint64_t file_id);
  static scoped_refptr<BlobDataItem> CreateFileFilesystem(
      const GURL& url,
      uint64_t offset,
      uint64_t length,
      base::Time expected_modification_time,
      scoped_refptr<FileSystemContext> file_system_context);
  static scoped_refptr<BlobDataItem> CreateReadableDataHandle(
      scoped_refptr<DataHandle> data_handle,
      uint64_t offset,
      uint64_t length);

  Type type() const { return type_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }

  base::span<const char> bytes() const {
    DCHECK_EQ(type_, Type::kBytes);
    return base::make_span(bytes_);
  }

  const base::FilePath& path() const {
    DCHECK_EQ(type_, Type::kFile);
    return path_;
  }

  const GURL& filesystem_url() const {
    DCHECK_EQ(type_, Type::kFileFilesystem);
    return filesystem_url_;
  }

  FileSystemContext* file_system_context() const {
    DCHECK_EQ(type_, Type::kFileFilesystem);
    return file_system_context_.get();
  }

  const base::Time& expected_modification_time() const {
    DCHECK(type_ == Type::kFile || type_ == Type::kFileFilesystem)
        << static_cast<int>(type_);
    return expected_modification_time_;
  }

  DataHandle* data_handle() const {
    DCHECK(type_ == Type::kFile || type_ == Type::kReadableDataHandle)
        << static_cast<int>(type_);
    return data_handle_.get();
  }

  // Returns true if this item was created by CreateFutureFile.
  bool IsFutureFileItem() const;
  // Returns |file_id| given to CreateFutureFile.
  uint64_t GetFutureFileID() const;

 private:
  friend class BlobBuilderFromStream;
  friend class BlobDataBuilder;
  friend class BlobStorageContext;
  friend class base::RefCounted<BlobDataItem>;
  friend COMPONENT_EXPORT(STORAGE_BROWSER) void PrintTo(const BlobDataItem& x,
                                                        ::std::ostream* os);

  BlobDataItem(Type type, uint64_t offset, uint64_t length);
  virtual ~BlobDataItem();

  base::span<char> mutable_bytes() {
    DCHECK_EQ(type_, Type::kBytes);
    return base::make_span(bytes_);
  }

  void AllocateBytes();
  void PopulateBytes(base::span<const char> data);
  void ShrinkBytes(size_t new_length);

  void PopulateFile(base::FilePath path,
                    base::Time expected_modification_time,
                    scoped_refptr<DataHandle> data_handle);
  void ShrinkFile(uint64_t new_length);
  void GrowFile(uint64_t new_length);
  void SetFileModificationTime(base::Time time) {
    DCHECK_EQ(type_, Type::kFile);
    expected_modification_time_ = time;
  }

  Type type_;
  uint64_t offset_;
  uint64_t length_;

  std::vector<char> bytes_;  // For Type::kBytes.
  base::FilePath path_;      // For Type::kFile.
  GURL filesystem_url_;      // For Type::kFileFilesystem.
  base::Time
      expected_modification_time_;  // For Type::kFile and kFileFilesystem.

  scoped_refptr<DataHandle>
      data_handle_;  // For Type::kFile and kReadableDataHandle.

  scoped_refptr<FileSystemContext>
      file_system_context_;  // For Type::kFileFilesystem.
};

COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator==(const BlobDataItem& a, const BlobDataItem& b);
COMPONENT_EXPORT(STORAGE_BROWSER)
bool operator!=(const BlobDataItem& a, const BlobDataItem& b);

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_DATA_ITEM_H_
