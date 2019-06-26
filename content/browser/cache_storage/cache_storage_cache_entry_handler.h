// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/util/type_safety/pass_key.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "content/common/content_export.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

enum class CacheStorageOwner;

// The state needed to pass when writing to a cache.
struct PutContext {
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;

  PutContext(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::FetchAPIResponsePtr response,
             blink::mojom::BlobPtr blob,
             uint64_t blob_size,
             blink::mojom::BlobPtr side_data_blob,
             uint64_t side_data_blob_size,
             int64_t trace_id);

  ~PutContext();

  // Provided by the constructor.
  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
  blink::mojom::BlobPtr blob;
  uint64_t blob_size;
  blink::mojom::BlobPtr side_data_blob;
  uint64_t side_data_blob_size;
  int64_t trace_id;

  // Provided while writing to the cache.
  ErrorCallback callback;
  ScopedWritableEntry cache_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

class CONTENT_EXPORT CacheStorageCacheEntryHandler {
 public:
  // The DiskCacheBlobEntry is a ref-counted object containing both
  // a disk_cache Entry and a Handle to the cache in which it lives.  This
  // blob entry can then be used to create a BlobDataItem::DataHandle by
  // calling CacheStorageCacheEntryHandle's MakeDataHandle().  This ensures
  // both the cache and the disk_cache entry live as long as the blob.
  //
  // The blob DataHandle always lives on the IO thread.  The DiskCacheBlobEntry
  // is held cross-sequence, but is always created and destroyed on the
  // cache_storage scheduler sequence. This ensure both the cache and the
  // disk_cache entry live as long as the blob.
  class DiskCacheBlobEntry
      : public base::RefCountedDeleteOnSequence<DiskCacheBlobEntry> {
   public:
    // Use |CacheStorageCacheEntryHandler::CreateDiskCacheBlobEntry|.
    DiskCacheBlobEntry(
        util::PassKey<CacheStorageCacheEntryHandler> key,
        base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
        CacheStorageCacheHandle cache_handle,
        disk_cache::ScopedEntryPtr disk_cache_entry);

    // Only callable on IO thread.
    int Read(scoped_refptr<net::IOBuffer> dst_buffer,
             CacheStorageCache::EntryIndex disk_cache_index,
             uint64_t offset,
             int bytes_to_read,
             base::OnceCallback<void(int)> callback);

    // Callable on any thread.
    int GetSize(CacheStorageCache::EntryIndex disk_cache_index) const;

    // Callable on any thread.
    void PrintTo(::std::ostream* os) const;

    // Only callable on the creation sequence.
    void Invalidate();

    // Only callable on the creation sequence.
    disk_cache::ScopedEntryPtr& disk_cache_entry();

   private:
    friend class base::DeleteHelper<DiskCacheBlobEntry>;
    friend class base::RefCountedDeleteOnSequence<DiskCacheBlobEntry>;
    ~DiskCacheBlobEntry();

    // Only callable on the creation sequence.
    void ReadOnSequence(scoped_refptr<net::IOBuffer> dst_buffer,
                        int disk_cache_index,
                        uint64_t offset,
                        int bytes_to_read,
                        base::OnceCallback<void(int)> callback);

    // Only callable on the creation sequence.
    int ReadOnSequenceInternal(scoped_refptr<net::IOBuffer> dst_buffer,
                               int disk_cache_index,
                               uint64_t offset,
                               int bytes_to_read,
                               base::OnceCallback<void(int)> callback);

    void DidReadOnSequence(base::OnceCallback<void(int)> callback, int result);

    // Accessed on any thread
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;

    // Only accessed on the creation sequence.
    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler_;
    base::Optional<CacheStorageCacheHandle> cache_handle_;
    disk_cache::ScopedEntryPtr disk_cache_entry_;

    // Accessed on any thread.
    std::atomic<bool> valid_;

    // Cached on the creation sequence so that they can be synchronously
    // read on the IO thread.
    const std::string key_;
    const int index_headers_size_;
    const int index_response_body_size_;
    const int index_side_data_size_;

    // For methods that should only be executed on the creation sequence.
    SEQUENCE_CHECKER(sequence_checker_);

    // Do not add a WeakPtrFactory since this class uses a cross-sequence
    // delete helper.

    DISALLOW_COPY_AND_ASSIGN(DiskCacheBlobEntry);
  };

  scoped_refptr<DiskCacheBlobEntry> CreateDiskCacheBlobEntry(
      CacheStorageCacheHandle cache_handle,
      disk_cache::ScopedEntryPtr disk_cache_entry);

  virtual ~CacheStorageCacheEntryHandler();

  virtual std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) = 0;
  virtual void PopulateResponseBody(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      blink::mojom::FetchAPIResponse* response) = 0;
  virtual void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                                   blink::mojom::FetchAPIRequest* request) = 0;

  static std::unique_ptr<CacheStorageCacheEntryHandler> CreateCacheEntryHandler(
      CacheStorageOwner owner,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  void InvalidateDiskCacheBlobEntrys();
  void EraseDiskCacheBlobEntry(DiskCacheBlobEntry* blob_entry);

 protected:
  CacheStorageCacheEntryHandler(
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Create a serialized blob from the given entry and disk_cache index.  This
  // blob will not have any side data.
  blink::mojom::SerializedBlobPtr CreateBlob(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index);

  // Create a serialized blob from the given entry and disk_cache indices.
  blink::mojom::SerializedBlobPtr CreateBlobWithSideData(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index,
      CacheStorageCache::EntryIndex side_data_disk_cache_index);

  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  // Every subclass should provide its own implementation to avoid partial
  // destruction.
  virtual base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() = 0;

  // We keep track of the DiskCacheBlobEntry instances to allow us to invalidate
  // them if the cache has to be deleted while there are still references to
  // data in it.
  std::set<DiskCacheBlobEntry*> blob_entries_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(CacheStorageCacheEntryHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
