// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

#include "base/guid.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/storage/cache_entry_handler_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

namespace {

// A |BlobDataItem::DataHandle| implementation that wraps a
// |DiskCacheBlobEntry|.  In addition, each |DataHandleImpl| maps the main
// and side data to particular disk_cache indices.
//
// The |DataHandleImpl| is a "readable" handle.  It overrides the virtual
// size and reading methods to access the underlying disk_cache entry.
class DataHandleImpl : public storage::BlobDataItem::DataHandle {
 public:
  DataHandleImpl(
      scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
          blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index,
      CacheStorageCache::EntryIndex side_data_disk_cache_index)
      : blob_entry_(std::move(blob_entry)),
        disk_cache_index_(disk_cache_index),
        side_data_disk_cache_index_(side_data_disk_cache_index) {}

  uint64_t GetSize() const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return blob_entry_->GetSize(disk_cache_index_);
  }

  int Read(scoped_refptr<net::IOBuffer> dst_buffer,
           uint64_t src_offset,
           int bytes_to_read,
           base::OnceCallback<void(int)> callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return blob_entry_->Read(std::move(dst_buffer), disk_cache_index_,
                             src_offset, bytes_to_read, std::move(callback));
  }

  uint64_t GetSideDataSize() const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (side_data_disk_cache_index_ == CacheStorageCache::INDEX_INVALID)
      return 0;
    return blob_entry_->GetSize(side_data_disk_cache_index_);
  }

  int ReadSideData(scoped_refptr<net::IOBuffer> dst_buffer,
                   base::OnceCallback<void(int)> callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (side_data_disk_cache_index_ == CacheStorageCache::INDEX_INVALID)
      return net::ERR_FAILED;
    return blob_entry_->Read(std::move(dst_buffer), side_data_disk_cache_index_,
                             /* offset= */ 0, GetSideDataSize(),
                             std::move(callback));
  }

  void PrintTo(::std::ostream* os) const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    blob_entry_->PrintTo(os);
    *os << ",disk_cache_index:" << disk_cache_index_;
  }

  const char* BytesReadHistogramLabel() const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return "DiskCache.CacheStorage";
  }

 private:
  ~DataHandleImpl() override = default;

  const scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
      blob_entry_;
  const CacheStorageCache::EntryIndex disk_cache_index_;
  const CacheStorageCache::EntryIndex side_data_disk_cache_index_;

  DISALLOW_COPY_AND_ASSIGN(DataHandleImpl);
};

void FinalizeBlobOnIOThread(
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry> blob_entry,
    CacheStorageCache::EntryIndex disk_cache_index,
    CacheStorageCache::EntryIndex side_data_disk_cache_index,
    std::string uuid,
    blink::mojom::BlobRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Just allow the blob mojo message pipe to automatically close if we're
  // shutting down.
  if (!blob_context)
    return;

  auto inner_handle = base::MakeRefCounted<DataHandleImpl>(
      std::move(blob_entry), disk_cache_index, side_data_disk_cache_index);
  auto blob_data = std::make_unique<storage::BlobDataBuilder>(uuid);
  blob_data->AppendReadableDataHandle(std::move(inner_handle));
  auto blob_handle = blob_context->AddFinishedBlob(std::move(blob_data));

  storage::BlobImpl::Create(std::move(blob_handle), std::move(request));
}

}  // namespace

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::DiskCacheBlobEntry(
    util::PassKey<CacheStorageCacheEntryHandler> key,
    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry)
    : base::RefCountedDeleteOnSequence<DiskCacheBlobEntry>(
          base::SequencedTaskRunnerHandle::Get()),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      entry_handler_(std::move(entry_handler)),
      cache_handle_(std::move(cache_handle)),
      disk_cache_entry_(std::move(disk_cache_entry)),
      valid_(true),
      key_(disk_cache_entry_->GetKey()),
      index_headers_size_(
          disk_cache_entry_->GetDataSize(CacheStorageCache::INDEX_HEADERS)),
      index_response_body_size_(disk_cache_entry_->GetDataSize(
          CacheStorageCache::INDEX_RESPONSE_BODY)),
      index_side_data_size_(
          disk_cache_entry_->GetDataSize(CacheStorageCache::INDEX_SIDE_DATA)) {}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Read(
    scoped_refptr<net::IOBuffer> dst_buffer,
    CacheStorageCache::EntryIndex disk_cache_index,
    uint64_t offset,
    int bytes_to_read,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!valid_)
    return net::ERR_CACHE_READ_FAILURE;

  if (task_runner_->RunsTasksInCurrentSequence()) {
    return ReadOnSequenceInternal(std::move(dst_buffer), disk_cache_index,
                                  offset, bytes_to_read, std::move(callback));
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DiskCacheBlobEntry::ReadOnSequence, this,
                                std::move(dst_buffer), disk_cache_index, offset,
                                bytes_to_read, std::move(callback)));
  return net::ERR_IO_PENDING;
}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::GetSize(
    CacheStorageCache::EntryIndex disk_cache_index) const {
  // Callable on any thread.
  if (!valid_)
    return 0;
  switch (disk_cache_index) {
    case CacheStorageCache::INDEX_INVALID:
      return 0;
    case CacheStorageCache::INDEX_HEADERS:
      return index_headers_size_;
    case CacheStorageCache::INDEX_RESPONSE_BODY:
      return index_response_body_size_;
    case CacheStorageCache::INDEX_SIDE_DATA:
      return index_side_data_size_;
  }
  NOTREACHED();
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::PrintTo(
    ::std::ostream* os) const {
  // Callable on any thread.
  if (valid_)
    *os << "disk_cache_key:" << key_;
  else
    *os << "<invalidated>";
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Invalidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  valid_ = false;
  cache_handle_ = base::nullopt;
  entry_handler_ = nullptr;
  disk_cache_entry_ = nullptr;
}

disk_cache::ScopedEntryPtr&
CacheStorageCacheEntryHandler::DiskCacheBlobEntry::disk_cache_entry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return disk_cache_entry_;
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::ReadOnSequence(
    scoped_refptr<net::IOBuffer> dst_buffer,
    int disk_cache_index,
    uint64_t offset,
    int bytes_to_read,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The disk_cache behavior of sometimes returning the result synchronously
  // and sometimes invoking the callback requires us to adapt our callback
  // to repeating here.  Ideally disk_cache should be refactored to always
  // report the result through the callback.
  auto adapted_callback = base::AdaptCallbackForRepeating(std::move(callback));

  int result = ReadOnSequenceInternal(std::move(dst_buffer), disk_cache_index,
                                      offset, bytes_to_read, adapted_callback);

  if (result == net::ERR_IO_PENDING)
    return;

  DidReadOnSequence(std::move(adapted_callback), result);
}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::ReadOnSequenceInternal(
    scoped_refptr<net::IOBuffer> dst_buffer,
    int disk_cache_index,
    uint64_t offset,
    int bytes_to_read,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!disk_cache_entry_) {
    return net::ERR_CACHE_READ_FAILURE;
  }

  return disk_cache_entry_->ReadData(
      disk_cache_index, offset, dst_buffer.get(), bytes_to_read,
      base::BindOnce(&DiskCacheBlobEntry::DidReadOnSequence, this,
                     std::move(callback)));
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::DidReadOnSequence(
    base::OnceCallback<void(int)> callback,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    std::move(callback).Run(result);
  } else {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(std::move(callback), result));
  }
}

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::~DiskCacheBlobEntry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (entry_handler_)
    entry_handler_->EraseDiskCacheBlobEntry(this);
}

PutContext::PutContext(blink::mojom::FetchAPIRequestPtr request,
                       blink::mojom::FetchAPIResponsePtr response,
                       blink::mojom::BlobPtr blob,
                       uint64_t blob_size,
                       blink::mojom::BlobPtr side_data_blob,
                       uint64_t side_data_blob_size,
                       int64_t trace_id)
    : request(std::move(request)),
      response(std::move(response)),
      blob(std::move(blob)),
      blob_size(blob_size),
      side_data_blob(std::move(side_data_blob)),
      side_data_blob_size(side_data_blob_size),
      trace_id(trace_id) {}

PutContext::~PutContext() = default;

// Default implemetation of CacheStorageCacheEntryHandler.
class CacheStorageCacheEntryHandlerImpl : public CacheStorageCacheEntryHandler {
 public:
  CacheStorageCacheEntryHandlerImpl(
      base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheStorageCacheEntryHandler(std::move(blob_context)) {}
  ~CacheStorageCacheEntryHandlerImpl() override = default;

  std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    blink::mojom::BlobPtr blob;
    uint64_t blob_size = blink::BlobUtils::kUnknownSize;
    blink::mojom::BlobPtr side_data_blob;
    uint64_t side_data_blob_size = blink::BlobUtils::kUnknownSize;

    if (response->blob) {
      blob.Bind(std::move(response->blob->blob));
      blob_size = response->blob->size;
    }
    if (response->side_data_blob) {
      side_data_blob.Bind(std::move(response->side_data_blob->blob));
      side_data_blob_size = response->side_data_blob->size;
    }

    return std::make_unique<PutContext>(
        std::move(request), std::move(response), std::move(blob), blob_size,
        std::move(side_data_blob), side_data_blob_size, trace_id);
  }

  void PopulateResponseBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                            blink::mojom::FetchAPIResponse* response) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response->blob = CreateBlobWithSideData(
        std::move(blob_entry), CacheStorageCache::INDEX_RESPONSE_BODY,
        CacheStorageCache::INDEX_SIDE_DATA);
  }

  void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                           blink::mojom::FetchAPIRequest* request) override {}

 private:
  base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<CacheStorageCacheEntryHandlerImpl> weak_ptr_factory_{
      this};
};

CacheStorageCacheEntryHandler::CacheStorageCacheEntryHandler(
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : blob_context_(blob_context) {}

scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
CacheStorageCacheEntryHandler::CreateDiskCacheBlobEntry(
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto blob_entry =
      base::MakeRefCounted<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>(
          util::PassKey<CacheStorageCacheEntryHandler>(), GetWeakPtr(),
          std::move(cache_handle), std::move(disk_cache_entry));
  DCHECK_EQ(blob_entries_.count(blob_entry.get()), 0u);
  blob_entries_.insert(blob_entry.get());
  return blob_entry;
}

CacheStorageCacheEntryHandler::~CacheStorageCacheEntryHandler() = default;

void CacheStorageCacheEntryHandler::InvalidateDiskCacheBlobEntrys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Calling Invalidate() can cause the CacheStorageCacheEntryHandler to be
  // destroyed. Be careful not to touch |this| after calling Invalidate().
  std::set<DiskCacheBlobEntry*> entries = std::move(blob_entries_);
  for (auto* entry : entries)
    entry->Invalidate();
}

void CacheStorageCacheEntryHandler::EraseDiskCacheBlobEntry(
    DiskCacheBlobEntry* blob_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blob_entries_.count(blob_entry), 0u);
  blob_entries_.erase(blob_entry);
}

// static
std::unique_ptr<CacheStorageCacheEntryHandler>
CacheStorageCacheEntryHandler::CreateCacheEntryHandler(
    CacheStorageOwner owner,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  switch (owner) {
    case CacheStorageOwner::kCacheAPI:
      return std::make_unique<CacheStorageCacheEntryHandlerImpl>(
          std::move(blob_context));
    case CacheStorageOwner::kBackgroundFetch:
      return std::make_unique<background_fetch::CacheEntryHandlerImpl>(
          std::move(blob_context));
  }
  NOTREACHED();
}

blink::mojom::SerializedBlobPtr CacheStorageCacheEntryHandler::CreateBlob(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    CacheStorageCache::EntryIndex disk_cache_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateBlobWithSideData(std::move(blob_entry), disk_cache_index,
                                CacheStorageCache::INDEX_INVALID);
}

blink::mojom::SerializedBlobPtr
CacheStorageCacheEntryHandler::CreateBlobWithSideData(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    CacheStorageCache::EntryIndex disk_cache_index,
    CacheStorageCache::EntryIndex side_data_disk_cache_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto blob = blink::mojom::SerializedBlob::New();
  blob->size = blob_entry->GetSize(disk_cache_index);
  blob->uuid = base::GenerateGUID();

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    FinalizeBlobOnIOThread(blob_context_, std::move(blob_entry),
                           disk_cache_index, side_data_disk_cache_index,
                           blob->uuid, MakeRequest(&blob->blob));
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&FinalizeBlobOnIOThread, blob_context_,
                       std::move(blob_entry), disk_cache_index,
                       side_data_disk_cache_index, blob->uuid,
                       MakeRequest(&blob->blob)));
  }

  return blob;
}

}  // namespace content
