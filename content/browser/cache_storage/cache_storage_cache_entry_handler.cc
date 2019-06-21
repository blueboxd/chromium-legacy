// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

#include "base/guid.h"
#include "base/optional.h"
#include "content/browser/background_fetch/storage/cache_entry_handler_impl.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

namespace {

constexpr int kInvalidSideDataIndex = -1;

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
      int disk_cache_index,
      int side_data_disk_cache_index)
      : blob_entry_(std::move(blob_entry)),
        disk_cache_index_(disk_cache_index),
        side_data_disk_cache_index_(side_data_disk_cache_index) {}

  uint64_t GetSize() const override {
    return blob_entry_->GetSize(disk_cache_index_);
  }

  int Read(scoped_refptr<net::IOBuffer> dst_buffer,
           uint64_t src_offset,
           int bytes_to_read,
           base::OnceCallback<void(int)> callback) override {
    return blob_entry_->Read(std::move(dst_buffer), disk_cache_index_,
                             src_offset, bytes_to_read, std::move(callback));
  }

  uint64_t GetSideDataSize() const override {
    if (side_data_disk_cache_index_ == kInvalidSideDataIndex)
      return 0;
    return blob_entry_->GetSize(side_data_disk_cache_index_);
  }

  int ReadSideData(scoped_refptr<net::IOBuffer> dst_buffer,
                   base::OnceCallback<void(int)> callback) override {
    if (side_data_disk_cache_index_ == kInvalidSideDataIndex)
      return net::ERR_FAILED;
    return blob_entry_->Read(std::move(dst_buffer), side_data_disk_cache_index_,
                             /* offset= */ 0, GetSideDataSize(),
                             std::move(callback));
  }

  void PrintTo(::std::ostream* os) const override {
    blob_entry_->PrintTo(os);
    *os << ",disk_cache_index:" << disk_cache_index_;
  }

  const char* BytesReadHistogramLabel() const override {
    return "DiskCache.CacheStorage";
  }

 private:
  ~DataHandleImpl() override = default;

  const scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
      blob_entry_;
  const int disk_cache_index_;
  const int side_data_disk_cache_index_;

  DISALLOW_COPY_AND_ASSIGN(DataHandleImpl);
};

}  // namespace

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::DiskCacheBlobEntry(
    util::PassKey<CacheStorageCacheEntryHandler> key,
    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry)
    : entry_handler_(std::move(entry_handler)),
      cache_handle_(std::move(cache_handle)),
      disk_cache_entry_(std::move(disk_cache_entry)) {}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Read(
    scoped_refptr<net::IOBuffer> dst_buffer,
    int disk_cache_index,
    uint64_t offset,
    int bytes_to_read,
    base::OnceCallback<void(int)> callback) {
  if (!disk_cache_entry_)
    return net::ERR_CACHE_READ_FAILURE;

  return disk_cache_entry_->ReadData(disk_cache_index, offset, dst_buffer.get(),
                                     bytes_to_read, std::move(callback));
}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::GetSize(
    int disk_cache_index) const {
  if (!disk_cache_entry_)
    return 0;
  return disk_cache_entry_->GetDataSize(disk_cache_index);
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::PrintTo(
    ::std::ostream* os) const {
  if (disk_cache_entry_)
    *os << "disk_cache_key:" << disk_cache_entry_->GetKey();
  else
    *os << "<invalidated>";
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Invalidate() {
  cache_handle_ = base::nullopt;
  entry_handler_ = nullptr;
  disk_cache_entry_ = nullptr;
}

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::~DiskCacheBlobEntry() {
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
      : CacheStorageCacheEntryHandler(std::move(blob_context)),
        weak_ptr_factory_(this) {}
  ~CacheStorageCacheEntryHandlerImpl() override = default;

  std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) override {
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
    disk_cache::Entry* entry = blob_entry->disk_cache_entry().get();
    DCHECK(entry);

    // Create a blob with the response body data.
    response->blob = blink::mojom::SerializedBlob::New();
    response->blob->size =
        entry->GetDataSize(CacheStorageCache::INDEX_RESPONSE_BODY);
    response->blob->uuid = base::GenerateGUID();
    auto blob_data =
        std::make_unique<storage::BlobDataBuilder>(response->blob->uuid);

    auto inner_handle = MakeDataHandleWithSideData(
        std::move(blob_entry), CacheStorageCache::INDEX_RESPONSE_BODY,
        CacheStorageCache::INDEX_SIDE_DATA);
    blob_data->AppendReadableDataHandle(std::move(inner_handle));
    auto blob_handle = blob_context_->AddFinishedBlob(std::move(blob_data));

    storage::BlobImpl::Create(std::move(blob_handle),
                              MakeRequest(&response->blob->blob));
  }

  void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                           blink::mojom::FetchAPIRequest* request) override {}

 private:
  base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<CacheStorageCacheEntryHandlerImpl> weak_ptr_factory_;
};

CacheStorageCacheEntryHandler::CacheStorageCacheEntryHandler(
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : blob_context_(blob_context) {}

scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
CacheStorageCacheEntryHandler::CreateDiskCacheBlobEntry(
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry) {
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
  // Calling Invalidate() can cause the CacheStorageCacheEntryHandler to be
  // destroyed. Be careful not to touch |this| after calling Invalidate().
  std::set<DiskCacheBlobEntry*> entries = std::move(blob_entries_);
  for (auto* entry : entries)
    entry->Invalidate();
}

void CacheStorageCacheEntryHandler::EraseDiskCacheBlobEntry(
    DiskCacheBlobEntry* blob_entry) {
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

// static
scoped_refptr<storage::BlobDataItem::DataHandle>
CacheStorageCacheEntryHandler::MakeDataHandle(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    int disk_cache_index) {
  return MakeDataHandleWithSideData(std::move(blob_entry), disk_cache_index,
                                    kInvalidSideDataIndex);
}

// static
scoped_refptr<storage::BlobDataItem::DataHandle>
CacheStorageCacheEntryHandler::MakeDataHandleWithSideData(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    int disk_cache_index,
    int side_data_disk_cache_index) {
  return base::MakeRefCounted<DataHandleImpl>(
      std::move(blob_entry), disk_cache_index, side_data_disk_cache_index);
}

}  // namespace content
