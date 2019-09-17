// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/generated_code_cache.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/common/url_constants.h"
#include "net/base/completion_once_callback.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kPrefix[] = "_key";
constexpr char kSeparator[] = " \n";

// We always expect to receive valid URLs that can be used as keys to the code
// cache. The relevant checks (for ex: resource_url is valid, origin_lock is
// not opque etc.,) must be done prior to requesting the code cache.
//
// This function doesn't enforce anything in the production code. It is here
// to make the assumptions explicit and to catch any errors when DCHECKs are
// enabled.
void CheckValidKeys(const GURL& resource_url, const GURL& origin_lock) {
  // If the resource url is invalid don't cache the code.
  DCHECK(resource_url.is_valid() && resource_url.SchemeIsHTTPOrHTTPS());

  // |origin_lock| should be either empty or should have Http/Https/chrome
  // schemes and it should not be a URL with opaque origin. Empty origin_locks
  // are allowed when the renderer is not locked to an origin.
  DCHECK(origin_lock.is_empty() ||
         ((origin_lock.SchemeIsHTTPOrHTTPS() ||
           origin_lock.SchemeIs(content::kChromeUIScheme)) &&
          !url::Origin::Create(origin_lock).opaque()));
}

// Generates the cache key for the given |resource_url| and the |origin_lock|.
//   |resource_url| is the url corresponding to the requested resource.
//   |origin_lock| is the origin that the renderer which requested this
//   resource is locked to.
// For example, if SitePerProcess is enabled and http://script.com/script1.js is
// requested by http://example.com, then http://script.com/script.js is the
// resource_url and http://example.com is the origin_lock.
//
// This returns the key by concatenating the serialized url and origin lock
// with a separator in between. |origin_lock| could be empty when renderer is
// not locked to an origin (ex: SitePerProcess is disabled) and it is safe to
// use only |resource_url| as the key in such cases.
std::string GetCacheKey(const GURL& resource_url, const GURL& origin_lock) {
  CheckValidKeys(resource_url, origin_lock);

  // Add a prefix _ so it can't be parsed as a valid URL.
  std::string key(kPrefix);
  // Remove reference, username and password sections of the URL.
  key.append(net::SimplifyUrlForRequest(resource_url).spec());
  // Add a separator between URL and origin to avoid any possibility of
  // attacks by crafting the URL. URLs do not contain any control ASCII
  // characters, and also space is encoded. So use ' \n' as a seperator.
  key.append(kSeparator);

  if (origin_lock.is_valid())
    key.append(net::SimplifyUrlForRequest(origin_lock).spec());
  return key;
}

constexpr int kResponseTimeSizeInBytes = sizeof(int64_t);

static_assert(mojo_base::BigBuffer::kMaxInlineBytes >=
                  2 * kResponseTimeSizeInBytes,
              "Buffer may not be large enough for response time");
static_assert(mojo_base::BigBuffer::kMaxInlineBytes <=
                  std::numeric_limits<int>::max(),
              "Buffer size calculations may overflow int");

// A net::IOBufferWithSize backed by a mojo_base::BigBuffer. Using BigBuffer
// as an IOBuffer allows us to avoid a copy. For large code, this can be slow.
class BigIOBuffer : public net::IOBufferWithSize {
 public:
  explicit BigIOBuffer(size_t size) : net::IOBufferWithSize(nullptr, size) {
    buffer_ = mojo_base::BigBuffer(size);
    data_ = reinterpret_cast<char*>(buffer_.data());
    DCHECK(data_);
  }
  mojo_base::BigBuffer TakeBuffer() { return std::move(buffer_); }

 protected:
  ~BigIOBuffer() override {
    // Storage is managed by BigBuffer. We must clear these before the base
    // class destructor runs.
    this->data_ = nullptr;
    this->size_ = 0UL;
  }

 private:
  mojo_base::BigBuffer buffer_;

  DISALLOW_COPY_AND_ASSIGN(BigIOBuffer);
};

}  // namespace

std::string GeneratedCodeCache::GetResourceURLFromKey(const std::string& key) {
  constexpr size_t kPrefixStringLen = base::size(kPrefix) - 1;
  // Only expect valid keys. All valid keys have a prefix and a separator.
  DCHECK_GE(key.length(), kPrefixStringLen);
  DCHECK_NE(key.find(kSeparator), std::string::npos);

  std::string resource_url =
      key.substr(kPrefixStringLen, key.find(kSeparator) - kPrefixStringLen);
  return resource_url;
}

void GeneratedCodeCache::CollectStatistics(
    GeneratedCodeCache::CacheEntryStatus status) {
  switch (cache_type_) {
    case GeneratedCodeCache::CodeCacheType::kJavaScript:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.JS.Behaviour", status);
      break;
    case GeneratedCodeCache::CodeCacheType::kWebAssembly:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolatedCodeCache.WASM.Behaviour", status);
      break;
  }
}

// Stores the information about a pending request while disk backend is
// being initialized.
class GeneratedCodeCache::PendingOperation {
 public:
  static std::unique_ptr<PendingOperation> CreateWritePendingOp(
      std::string key,
      scoped_refptr<net::IOBufferWithSize>);
  static std::unique_ptr<PendingOperation> CreateFetchPendingOp(
      std::string key,
      const ReadDataCallback&);
  static std::unique_ptr<PendingOperation> CreateDeletePendingOp(
      std::string key);
  static std::unique_ptr<PendingOperation> CreateGetBackendPendingOp(
      GetBackendCallback callback);

  ~PendingOperation();

  Operation operation() const { return op_; }
  const std::string& key() const { return key_; }
  const scoped_refptr<net::IOBufferWithSize> data() const { return data_; }
  ReadDataCallback ReleaseReadCallback() { return std::move(read_callback_); }
  GetBackendCallback ReleaseBackendCallback() {
    return std::move(backend_callback_);
  }

 private:
  PendingOperation(Operation op,
                   std::string key,
                   scoped_refptr<net::IOBufferWithSize>,
                   const ReadDataCallback&,
                   GetBackendCallback);

  const Operation op_;
  const std::string key_;
  const scoped_refptr<net::IOBufferWithSize> data_;
  ReadDataCallback read_callback_;
  GetBackendCallback backend_callback_;
};

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateWritePendingOp(
    std::string key,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  return base::WrapUnique(
      new PendingOperation(Operation::kWrite, std::move(key), buffer,
                           ReadDataCallback(), GetBackendCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateFetchPendingOp(
    std::string key,
    const ReadDataCallback& read_callback) {
  return base::WrapUnique(new PendingOperation(
      Operation::kFetch, std::move(key), scoped_refptr<net::IOBufferWithSize>(),
      std::move(read_callback), GetBackendCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateDeletePendingOp(std::string key) {
  return base::WrapUnique(
      new PendingOperation(Operation::kDelete, std::move(key),
                           scoped_refptr<net::IOBufferWithSize>(),
                           ReadDataCallback(), GetBackendCallback()));
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::PendingOperation::CreateGetBackendPendingOp(
    GetBackendCallback backend_callback) {
  return base::WrapUnique(
      new PendingOperation(Operation::kGetBackend, std::string(),
                           scoped_refptr<net::IOBufferWithSize>(),
                           ReadDataCallback(), std::move(backend_callback)));
}

GeneratedCodeCache::PendingOperation::PendingOperation(
    Operation op,
    std::string key,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const ReadDataCallback& read_callback,
    GetBackendCallback backend_callback)
    : op_(op),
      key_(std::move(key)),
      data_(buffer),
      read_callback_(std::move(read_callback)),
      backend_callback_(std::move(backend_callback)) {}

GeneratedCodeCache::PendingOperation::~PendingOperation() = default;

GeneratedCodeCache::GeneratedCodeCache(const base::FilePath& path,
                                       int max_size_bytes,
                                       CodeCacheType cache_type)
    : backend_state_(kInitializing),
      path_(path),
      max_size_bytes_(max_size_bytes),
      cache_type_(cache_type) {
  CreateBackend();
}

GeneratedCodeCache::~GeneratedCodeCache() = default;

void GeneratedCodeCache::GetBackend(GetBackendCallback callback) {
  switch (backend_state_) {
    case kFailed:
      std::move(callback).Run(nullptr);
      return;
    case kInitialized:
      std::move(callback).Run(backend_.get());
      return;
    case kInitializing:
      pending_ops_.push_back(
          GeneratedCodeCache::PendingOperation::CreateGetBackendPendingOp(
              std::move(callback)));
      return;
  }
}

void GeneratedCodeCache::WriteData(const GURL& url,
                                   const GURL& origin_lock,
                                   const base::Time& response_time,
                                   base::span<const uint8_t> data) {
  // Silently ignore the requests.
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  // Append the response time to the metadata. Code caches store
  // response_time + generated code as a single entry.
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(data.size() +
                                                  kResponseTimeSizeInBytes);
  int64_t serialized_time =
      response_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  memcpy(buffer->data(), &serialized_time, kResponseTimeSizeInBytes);
  if (!data.empty())
    memcpy(buffer->data() + kResponseTimeSizeInBytes, data.data(), data.size());

  std::string key = GetCacheKey(url, origin_lock);
  // If there is an in progress operation corresponding to this key. Enqueue it
  // so we can issue once the in-progress operation finishes.
  if (!TryBeginOperation(key)) {
    EnqueueAsPendingOperation(
        key, GeneratedCodeCache::PendingOperation::CreateWritePendingOp(
                 key, buffer));
    return;
  }

  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateWritePendingOp(
            std::move(key), buffer));
    return;
  }

  WriteDataImpl(key, buffer);
}

void GeneratedCodeCache::FetchEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    ReadDataCallback read_data_callback) {
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    // Silently ignore the requests.
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  // If there is an in progress operation corresponding to this key. Enqueue it
  // so we can issue once the in-progress operation finishes.
  if (!TryBeginOperation(key)) {
    EnqueueAsPendingOperation(
        key, GeneratedCodeCache::PendingOperation::CreateFetchPendingOp(
                 key, read_data_callback));
    return;
  }

  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateFetchPendingOp(
            std::move(key), read_data_callback));
    return;
  }

  FetchEntryImpl(key, read_data_callback);
}

void GeneratedCodeCache::DeleteEntry(const GURL& url, const GURL& origin_lock) {
  // Silently ignore the requests.
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.push_back(
        GeneratedCodeCache::PendingOperation::CreateDeletePendingOp(
            std::move(key)));
    return;
  }

  DeleteEntryImpl(key);
}

void GeneratedCodeCache::CreateBackend() {
  // Create a new Backend pointer that cleans itself if the GeneratedCodeCache
  // instance is not live when the CreateCacheBackend finishes.
  scoped_refptr<base::RefCountedData<ScopedBackendPtr>> shared_backend_ptr =
      new base::RefCountedData<ScopedBackendPtr>();

  net::CompletionOnceCallback create_backend_complete =
      base::BindOnce(&GeneratedCodeCache::DidCreateBackend,
                     weak_ptr_factory_.GetWeakPtr(), shared_backend_ptr);

  // If the initialization of the existing cache fails, this call would delete
  // all the contents and recreates a new one.
  int rv = disk_cache::CreateCacheBackend(
      cache_type_ == GeneratedCodeCache::CodeCacheType::kJavaScript
          ? net::GENERATED_BYTE_CODE_CACHE
          : net::GENERATED_NATIVE_CODE_CACHE,
      net::CACHE_BACKEND_SIMPLE, path_, max_size_bytes_, true, nullptr,
      &shared_backend_ptr->data, std::move(create_backend_complete));
  if (rv != net::ERR_IO_PENDING) {
    DidCreateBackend(shared_backend_ptr, rv);
  }
}

void GeneratedCodeCache::DidCreateBackend(
    scoped_refptr<base::RefCountedData<ScopedBackendPtr>> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    backend_state_ = kFailed;
    // Process pending operations to process any required callbacks.
    IssuePendingOperations();
    return;
  }

  backend_ = std::move(backend_ptr->data);
  backend_state_ = kInitialized;
  IssuePendingOperations();
}

void GeneratedCodeCache::IssuePendingOperations() {
  // Issue all the pending operations that were received when creating
  // the backend.
  for (auto const& op : pending_ops_) {
    IssueOperation(op.get());
  }
  pending_ops_.clear();
}

void GeneratedCodeCache::IssueOperation(PendingOperation* op) {
  switch (op->operation()) {
    case kFetch:
      FetchEntryImpl(op->key(), op->ReleaseReadCallback());
      break;
    case kWrite:
      WriteDataImpl(op->key(), op->data());
      break;
    case kDelete:
      DeleteEntryImpl(op->key());
      break;
    case kGetBackend:
      DoPendingGetBackend(op->ReleaseBackendCallback());
      break;
  }
}

void GeneratedCodeCache::WriteDataImpl(
    const std::string& key,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  if (backend_state_ != kInitialized) {
    IssueQueuedOperationForEntry(key);
    return;
  }

  disk_cache::EntryResultCallback callback =
      base::BindOnce(&GeneratedCodeCache::CompleteForWriteData,
                     weak_ptr_factory_.GetWeakPtr(), buffer, key);

  disk_cache::EntryResult result =
      backend_->OpenOrCreateEntry(key, net::LOW, std::move(callback));
  if (result.net_error() != net::ERR_IO_PENDING) {
    CompleteForWriteData(buffer, key, std::move(result));
  }
}

void GeneratedCodeCache::CompleteForWriteData(
    scoped_refptr<net::IOBufferWithSize> buffer,
    const std::string& key,
    disk_cache::EntryResult entry_result) {
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(CacheEntryStatus::kError);
    IssueQueuedOperationForEntry(key);
    return;
  }

  int result = net::ERR_FAILED;
  bool opened = entry_result.opened();
  {
    disk_cache::ScopedEntryPtr disk_entry(entry_result.ReleaseEntry());

    if (opened) {
      CollectStatistics(CacheEntryStatus::kUpdate);
    } else {
      CollectStatistics(CacheEntryStatus::kCreate);
    }
    // This call will truncate the data. This is safe to do since we read the
    // entire data at the same time currently. If we want to read in parts we
    // have to doom the entry first.
    result = disk_entry->WriteData(
        kDataIndex, 0, buffer.get(), buffer->size(),
        base::BindOnce(&GeneratedCodeCache::WriteDataCompleted,
                       weak_ptr_factory_.GetWeakPtr(), key),
        true);
  }
  if (result != net::ERR_IO_PENDING) {
    WriteDataCompleted(key, result);
  }
}

void GeneratedCodeCache::WriteDataCompleted(const std::string& key, int rv) {
  if (rv < 0) {
    CollectStatistics(CacheEntryStatus::kWriteFailed);
    // The write failed; we should delete the entry.
    DeleteEntryImpl(key);
  }
  IssueQueuedOperationForEntry(key);
}

void GeneratedCodeCache::FetchEntryImpl(const std::string& key,
                                        ReadDataCallback read_data_callback) {
  if (backend_state_ != kInitialized) {
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
    IssueQueuedOperationForEntry(key);
    return;
  }

  disk_cache::EntryResultCallback callback =
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForReadData,
                     weak_ptr_factory_.GetWeakPtr(), read_data_callback, key);

  // This is a part of loading cycle and hence should run with a high priority.
  disk_cache::EntryResult result =
      backend_->OpenEntry(key, net::HIGHEST, std::move(callback));
  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForReadData(read_data_callback, key, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForReadData(
    ReadDataCallback read_data_callback,
    const std::string& key,
    disk_cache::EntryResult entry_result) {
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(CacheEntryStatus::kMiss);
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
    IssueQueuedOperationForEntry(key);
    return;
  }

  disk_cache::ScopedEntryPtr disk_entry(entry_result.ReleaseEntry());
  // There should be a valid entry if the open was successful.
  DCHECK(disk_entry);

  int entry_size = disk_entry->GetDataSize(kDataIndex);
  // Use a BigIOBuffer backed to read and transfer the entry without copying.
  // We have to read the data in two parts, response time and code, if we don't
  // want to copy. Use the same buffer to read the response time and the code.
  int code_size =
      std::max(kResponseTimeSizeInBytes, entry_size - kResponseTimeSizeInBytes);
  // Release the disk entry to pass it to |ReadResponseTimeComplete|.
  disk_cache::Entry* entry = disk_entry.release();
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<BigIOBuffer>(static_cast<size_t>(code_size));
  net::CompletionOnceCallback callback = base::BindOnce(
      &GeneratedCodeCache::ReadResponseTimeComplete,
      weak_ptr_factory_.GetWeakPtr(), key, read_data_callback, buffer, entry);
  int result = entry->ReadData(kDataIndex, 0, buffer.get(),
                               kResponseTimeSizeInBytes, std::move(callback));
  if (result != net::ERR_IO_PENDING) {
    ReadResponseTimeComplete(key, read_data_callback, buffer, entry, result);
  }
}

void GeneratedCodeCache::ReadResponseTimeComplete(
    const std::string& key,
    ReadDataCallback read_data_callback,
    scoped_refptr<net::IOBufferWithSize> buffer,
    disk_cache::Entry* entry,
    int rv) {
  DCHECK(entry);
  disk_cache::ScopedEntryPtr disk_entry(entry);
  if (rv != kResponseTimeSizeInBytes) {
    CollectStatistics(CacheEntryStatus::kMiss);
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
  } else {
    // This is considered a cache hit, since response time was read.
    CollectStatistics(CacheEntryStatus::kHit);
    int64_t raw_response_time = *(reinterpret_cast<int64_t*>(buffer->data()));
    net::CompletionOnceCallback callback = base::BindOnce(
        &GeneratedCodeCache::ReadCodeComplete, weak_ptr_factory_.GetWeakPtr(),
        key, read_data_callback, buffer, raw_response_time);
    int result =
        disk_entry->ReadData(kDataIndex, kResponseTimeSizeInBytes, buffer.get(),
                             buffer->size(), std::move(callback));
    if (result != net::ERR_IO_PENDING) {
      ReadCodeComplete(key, read_data_callback, buffer, raw_response_time,
                       result);
    }
  }
}

void GeneratedCodeCache::ReadCodeComplete(
    const std::string& key,
    ReadDataCallback callback,
    scoped_refptr<net::IOBufferWithSize> buffer,
    int64_t raw_response_time,
    int rv) {
  base::Time response_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(raw_response_time));
  if (rv != buffer->size()) {
    // Trim the buffer in the unlikely case that code size is less than
    // kResponseTimeSizeInBytes. On error, return an empty buffer with the
    // response time, so the renderer can clear the metadata.
    mojo_base::BigBuffer trimmed_buffer =
        rv > 0 ? mojo_base::BigBuffer(base::make_span(
                     reinterpret_cast<const uint8_t*>(buffer->data()),
                     static_cast<size_t>(rv)))
               : mojo_base::BigBuffer();
    std::move(callback).Run(response_time, std::move(trimmed_buffer));
  } else {
    std::move(callback).Run(
        response_time, static_cast<BigIOBuffer*>(buffer.get())->TakeBuffer());
  }
  IssueQueuedOperationForEntry(key);
}

void GeneratedCodeCache::DeleteEntryImpl(const std::string& key) {
  if (backend_state_ != kInitialized)
    return;

  CollectStatistics(CacheEntryStatus::kClear);
  backend_->DoomEntry(key, net::LOWEST, net::CompletionOnceCallback());
}

void GeneratedCodeCache::IssueQueuedOperationForEntry(const std::string& key) {
  auto it = active_entries_map_.find(key);
  DCHECK(it != active_entries_map_.end());

  // If no more queued entries then remove the entry to indicate that there are
  // no in-progress operations for this key.
  if (it->second.empty()) {
    active_entries_map_.erase(it);
    return;
  }

  std::unique_ptr<PendingOperation> op = std::move(it->second.front());
  // Pop it before issuing the operation. Still retain the queue even if it is
  // empty to indicate that there is a in-progress operation.
  it->second.pop();
  IssueOperation(op.get());
}

bool GeneratedCodeCache::TryBeginOperation(const std::string& key) {
  auto it = active_entries_map_.find(key);
  if (it != active_entries_map_.end())
    return false;

  // Create an entry to indicate there is a in-progress operation for this key.
  active_entries_map_[key] = base::queue<std::unique_ptr<PendingOperation>>();
  return true;
}

void GeneratedCodeCache::EnqueueAsPendingOperation(
    const std::string& key,
    std::unique_ptr<PendingOperation> op) {
  auto it = active_entries_map_.find(key);
  DCHECK(it != active_entries_map_.end());
  it->second.emplace(std::move(op));
}

void GeneratedCodeCache::DoPendingGetBackend(GetBackendCallback user_callback) {
  if (backend_state_ == kInitialized) {
    std::move(user_callback).Run(backend_.get());
    return;
  }

  DCHECK_EQ(backend_state_, kFailed);
  std::move(user_callback).Run(nullptr);
  return;
}

void GeneratedCodeCache::SetLastUsedTimeForTest(
    const GURL& resource_url,
    const GURL& origin_lock,
    base::Time time,
    base::RepeatingCallback<void(void)> user_callback) {
  // This is used only for tests. So reasonable to assume that backend is
  // initialized here. All other operations handle the case when backend was not
  // yet opened.
  DCHECK_EQ(backend_state_, kInitialized);

  disk_cache::EntryResultCallback callback =
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForSetLastUsedForTest,
                     weak_ptr_factory_.GetWeakPtr(), time, user_callback);

  std::string key = GetCacheKey(resource_url, origin_lock);
  disk_cache::EntryResult result =
      backend_->OpenEntry(key, net::LOWEST, std::move(callback));
  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForSetLastUsedForTest(time, user_callback, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForSetLastUsedForTest(
    base::Time time,
    base::RepeatingCallback<void(void)> callback,
    disk_cache::EntryResult result) {
  DCHECK_EQ(result.net_error(), net::OK);
  {
    disk_cache::ScopedEntryPtr disk_entry(result.ReleaseEntry());
    DCHECK(disk_entry);
    disk_entry->SetLastUsedTimeForTest(time);
  }
  std::move(callback).Run();
}

}  // namespace content
