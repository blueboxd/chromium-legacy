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
constexpr int kDataSizeInBytes = sizeof(uint32_t);
constexpr int kHeaderSizeInBytes = kResponseTimeSizeInBytes + kDataSizeInBytes;
// This is the threshold for storing the header and cached code in stream 0,
// which is read into memory on opening an entry. JavaScript code caching stores
// time stamps with no data, or timestamps with just a tag, and we observe many
// 8 and 16 byte reads and writes. Make the threshold larger to speed up many
// code entries too.
constexpr int kSmallDataLimit = 4096;

void WriteSmallDataHeader(scoped_refptr<net::IOBufferWithSize> buffer,
                          const base::Time& response_time,
                          uint32_t data_size) {
  DCHECK_LE(kHeaderSizeInBytes, buffer->size());
  int64_t serialized_time =
      response_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  memcpy(buffer->data(), &serialized_time, kResponseTimeSizeInBytes);
  // Copy size to small data buffer.
  memcpy(buffer->data() + kResponseTimeSizeInBytes, &data_size,
         kDataSizeInBytes);
}

void ReadSmallDataHeader(scoped_refptr<net::IOBufferWithSize> buffer,
                         base::Time* response_time,
                         uint32_t* data_size) {
  DCHECK_LE(kHeaderSizeInBytes, buffer->size());
  int64_t raw_response_time = *(reinterpret_cast<int64_t*>(buffer->data()));
  *response_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(raw_response_time));
  *data_size =
      *(reinterpret_cast<uint32_t*>(buffer->data() + kResponseTimeSizeInBytes));
}

static_assert(mojo_base::BigBuffer::kMaxInlineBytes <=
                  std::numeric_limits<int>::max(),
              "Buffer size calculations may overflow int");

// A net::IOBufferWithSize backed by a mojo_base::BigBuffer. Using BigBuffer
// as an IOBuffer allows us to avoid a copy. For large code, this can be slow.
class BigIOBuffer : public net::IOBufferWithSize {
 public:
  explicit BigIOBuffer(mojo_base::BigBuffer buffer)
      : net::IOBufferWithSize(nullptr, buffer.size()),
        buffer_(std::move(buffer)) {
    data_ = reinterpret_cast<char*>(buffer_.data());
  }
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
// being initialized or another request for the same key is live.
class GeneratedCodeCache::PendingOperation {
 public:
  PendingOperation(Operation op,
                   const std::string& key,
                   scoped_refptr<net::IOBufferWithSize> small_buffer,
                   scoped_refptr<BigIOBuffer> large_buffer)
      : op_(op),
        key_(key),
        small_buffer_(small_buffer),
        large_buffer_(large_buffer) {
    DCHECK_EQ(Operation::kWrite, op_);
  }

  PendingOperation(Operation op,
                   const std::string& key,
                   ReadDataCallback read_callback)
      : op_(op), key_(key), read_callback_(std::move(read_callback)) {
    DCHECK_EQ(Operation::kFetch, op_);
  }

  PendingOperation(Operation op, const std::string& key) : op_(op), key_(key) {
    DCHECK_EQ(Operation::kDelete, op_);
  }

  PendingOperation(Operation op, GetBackendCallback backend_callback)
      : op_(op), backend_callback_(std::move(backend_callback)) {
    DCHECK_EQ(Operation::kGetBackend, op_);
  }

  ~PendingOperation();

  Operation operation() const { return op_; }
  const std::string& key() const { return key_; }
  scoped_refptr<net::IOBufferWithSize> small_buffer() { return small_buffer_; }
  scoped_refptr<BigIOBuffer> large_buffer() { return large_buffer_; }
  ReadDataCallback TakeReadCallback() { return std::move(read_callback_); }
  GetBackendCallback TakeBackendCallback() {
    return std::move(backend_callback_);
  }

  // These are called by Fetch operations to hold the buffers we create once the
  // entry is opened.
  void set_small_buffer(scoped_refptr<net::IOBufferWithSize> small_buffer) {
    DCHECK_EQ(Operation::kFetch, op_);
    small_buffer_ = small_buffer;
  }
  void set_large_buffer(scoped_refptr<BigIOBuffer> large_buffer) {
    DCHECK_EQ(Operation::kFetch, op_);
    large_buffer_ = large_buffer;
  }

  // Verifies that Write/Fetch callbacks are received in the order we expect.
  void VerifyCompletions(int expected) {
#if DCHECK_IS_ON()
    DCHECK_EQ(expected, completions_);
    completions_++;
#endif
  }

 private:
  const Operation op_;
  const std::string key_;
  scoped_refptr<net::IOBufferWithSize> small_buffer_;
  scoped_refptr<BigIOBuffer> large_buffer_;
  ReadDataCallback read_callback_;
  GetBackendCallback backend_callback_;
#if DCHECK_IS_ON()
  int completions_ = 0;
#endif
};

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
      pending_ops_.emplace(std::make_unique<PendingOperation>(
          Operation::kGetBackend, std::move(callback)));
      return;
  }
}

void GeneratedCodeCache::WriteEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    const base::Time& response_time,
                                    mojo_base::BigBuffer data) {
  if (backend_state_ == kFailed) {
    // Silently fail the request.
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  // If data is small, combine the header and data into a single write.
  scoped_refptr<net::IOBufferWithSize> small_buffer;
  scoped_refptr<BigIOBuffer> large_buffer;
  uint32_t data_size = static_cast<uint32_t>(data.size());
  if (data_size <= kSmallDataLimit) {
    small_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
        kHeaderSizeInBytes + data.size());
    // Copy |data| into the small buffer.
    memcpy(small_buffer->data() + kHeaderSizeInBytes, data.data(), data.size());
    // We write 0 bytes and truncate stream 1 to clear any stale data.
    large_buffer = base::MakeRefCounted<BigIOBuffer>(mojo_base::BigBuffer());
  } else {
    small_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(kHeaderSizeInBytes);
    large_buffer = base::MakeRefCounted<BigIOBuffer>(std::move(data));
  }
  WriteSmallDataHeader(small_buffer, response_time, data_size);

  // Create the write operation.
  std::string key = GetCacheKey(url, origin_lock);
  auto op = std::make_unique<PendingOperation>(Operation::kWrite, key,
                                               small_buffer, large_buffer);

  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.emplace(std::move(op));
    return;
  }

  EnqueueOperationAndIssueIfNext(std::move(op));
}

void GeneratedCodeCache::FetchEntry(const GURL& url,
                                    const GURL& origin_lock,
                                    ReadDataCallback read_data_callback) {
  if (backend_state_ == kFailed) {
    CollectStatistics(CacheEntryStatus::kError);
    // Fail the request.
    std::move(read_data_callback).Run(base::Time(), mojo_base::BigBuffer());
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  auto op = std::make_unique<PendingOperation>(Operation::kFetch, key,
                                               std::move(read_data_callback));
  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.emplace(std::move(op));
    return;
  }

  EnqueueOperationAndIssueIfNext(std::move(op));
}

void GeneratedCodeCache::DeleteEntry(const GURL& url, const GURL& origin_lock) {
  if (backend_state_ == kFailed) {
    // Silently fail.
    CollectStatistics(CacheEntryStatus::kError);
    return;
  }

  std::string key = GetCacheKey(url, origin_lock);
  auto op = std::make_unique<PendingOperation>(Operation::kDelete, key);

  if (backend_state_ != kInitialized) {
    // Insert it into the list of pending operations while the backend is
    // still being opened.
    pending_ops_.emplace(std::move(op));
    return;
  }

  EnqueueOperationAndIssueIfNext(std::move(op));
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
  } else {
    backend_ = std::move(backend_ptr->data);
    backend_state_ = kInitialized;
  }
  IssuePendingOperations();
}

void GeneratedCodeCache::IssuePendingOperations() {
  // Issue any operations that were received while creating the backend.
  while (!pending_ops_.empty()) {
    // Take ownership of the next PendingOperation here. |op| will either be
    // moved onto a queue in active_entries_map_ or issued and completed in
    // |DoPendingGetBackend|.
    std::unique_ptr<PendingOperation> op = std::move(pending_ops_.front());
    pending_ops_.pop();
    // Properly enqueue/dequeue ops for Write, Fetch, and Delete.
    if (op->operation() != Operation::kGetBackend) {
      EnqueueOperationAndIssueIfNext(std::move(op));
    } else {
      // There is no queue for get backend operations. Issue them immediately.
      IssueOperation(op.get());
    }
  }
}

void GeneratedCodeCache::IssueOperation(PendingOperation* op) {
  switch (op->operation()) {
    case kFetch:
      FetchEntryImpl(op);
      break;
    case kWrite:
      WriteEntryImpl(op);
      break;
    case kDelete:
      DeleteEntryImpl(op);
      break;
    case kGetBackend:
      DoPendingGetBackend(op);
      break;
  }
}

void GeneratedCodeCache::WriteEntryImpl(PendingOperation* op) {
  DCHECK_EQ(Operation::kWrite, op->operation());
  if (backend_state_ != kInitialized) {
    // Silently fail the request.
    CloseOperationAndIssueNext(op);
    return;
  }

  disk_cache::EntryResult result = backend_->OpenOrCreateEntry(
      op->key(), net::LOW,
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForWrite,
                     weak_ptr_factory_.GetWeakPtr(), op));

  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForWrite(op, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForWrite(
    PendingOperation* op,
    disk_cache::EntryResult entry_result) {
  DCHECK_EQ(Operation::kWrite, op->operation());
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(CacheEntryStatus::kError);
    CloseOperationAndIssueNext(op);
    return;
  }

  if (entry_result.opened()) {
    CollectStatistics(CacheEntryStatus::kUpdate);
  } else {
    CollectStatistics(CacheEntryStatus::kCreate);
  }

  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());
  // There should be a valid entry if the open was successful.
  DCHECK(entry);

  // Write the small data first, truncating.
  auto small_buffer = op->small_buffer();
  int result = entry->WriteData(
      kSmallDataStream, 0, small_buffer.get(), small_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::WriteSmallBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op),
      true);

  if (result != net::ERR_IO_PENDING) {
    WriteSmallBufferComplete(op, result);
  }

  // Write the large data, truncating.
  auto large_buffer = op->large_buffer();
  result = entry->WriteData(
      kLargeDataStream, 0, large_buffer.get(), large_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::WriteLargeBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op),
      true);

  if (result != net::ERR_IO_PENDING) {
    WriteLargeBufferComplete(op, result);
  }
}

void GeneratedCodeCache::WriteSmallBufferComplete(PendingOperation* op,
                                                  int rv) {
  DCHECK_EQ(Operation::kWrite, op->operation());
  op->VerifyCompletions(0);  // WriteLargeBufferComplete did not run.
  if (rv != op->small_buffer()->size()) {
    // The small data write failed; release the small buffer to signal that
    // the overall request should also fail.
    op->set_small_buffer(nullptr);
  }
  // |WriteLargeBufferComplete| must run and call CloseOperationAndIssueNext.
}

void GeneratedCodeCache::WriteLargeBufferComplete(PendingOperation* op,
                                                  int rv) {
  DCHECK_EQ(Operation::kWrite, op->operation());
  op->VerifyCompletions(1);  // WriteSmallBufferComplete ran.
  if (rv != op->large_buffer()->size() || !op->small_buffer()) {
    // The write failed; record the failure and doom the entry here.
    CollectStatistics(CacheEntryStatus::kWriteFailed);
    DoomEntry(op);
  }
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::FetchEntryImpl(PendingOperation* op) {
  DCHECK_EQ(Operation::kFetch, op->operation());
  if (backend_state_ != kInitialized) {
    op->TakeReadCallback().Run(base::Time(), mojo_base::BigBuffer());
    CloseOperationAndIssueNext(op);
    return;
  }

  // This is a part of loading cycle and hence should run with a high priority.
  disk_cache::EntryResult result = backend_->OpenEntry(
      op->key(), net::HIGHEST,
      base::BindOnce(&GeneratedCodeCache::OpenCompleteForRead,
                     weak_ptr_factory_.GetWeakPtr(), op));
  if (result.net_error() != net::ERR_IO_PENDING) {
    OpenCompleteForRead(op, std::move(result));
  }
}

void GeneratedCodeCache::OpenCompleteForRead(
    PendingOperation* op,
    disk_cache::EntryResult entry_result) {
  DCHECK_EQ(Operation::kFetch, op->operation());
  if (entry_result.net_error() != net::OK) {
    CollectStatistics(CacheEntryStatus::kMiss);
    op->TakeReadCallback().Run(base::Time(), mojo_base::BigBuffer());
    CloseOperationAndIssueNext(op);
    return;
  }

  disk_cache::ScopedEntryPtr entry(entry_result.ReleaseEntry());
  // There should be a valid entry if the open was successful.
  DCHECK(entry);

  int small_size = entry->GetDataSize(kSmallDataStream);
  scoped_refptr<net::IOBufferWithSize> small_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(small_size);
  op->set_small_buffer(small_buffer);
  int large_size = entry->GetDataSize(kLargeDataStream);
  scoped_refptr<BigIOBuffer> large_buffer =
      base::MakeRefCounted<BigIOBuffer>(large_size);
  op->set_large_buffer(large_buffer);

  // Read the small data first.
  int result = entry->ReadData(
      kSmallDataStream, 0, small_buffer.get(), small_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::ReadSmallBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op));

  if (result != net::ERR_IO_PENDING) {
    ReadSmallBufferComplete(op, result);
  }

  // Skip the large read if data is in the small read.
  if (large_size == 0)
    return;

  // Read the large data.
  result = entry->ReadData(
      kLargeDataStream, 0, large_buffer.get(), large_buffer->size(),
      base::BindOnce(&GeneratedCodeCache::ReadLargeBufferComplete,
                     weak_ptr_factory_.GetWeakPtr(), op));
  if (result != net::ERR_IO_PENDING) {
    ReadLargeBufferComplete(op, result);
  }
}

void GeneratedCodeCache::ReadSmallBufferComplete(PendingOperation* op, int rv) {
  DCHECK_EQ(Operation::kFetch, op->operation());
  op->VerifyCompletions(0);  // ReadLargeBufferComplete did not run.
  if (rv != op->small_buffer()->size() || rv < kHeaderSizeInBytes) {
    CollectStatistics(CacheEntryStatus::kMiss);
    // The small data stream read failed or is incomplete; release the buffer
    // to signal that the overall request should also fail.
    op->set_small_buffer(nullptr);
  } else {
    // This is considered a cache hit, since the small data was read.
    CollectStatistics(CacheEntryStatus::kHit);
  }
  // Small reads must finish now since no large read is pending.
  if (op->large_buffer()->size() == 0)
    ReadLargeBufferComplete(op, 0);
}

void GeneratedCodeCache::ReadLargeBufferComplete(PendingOperation* op, int rv) {
  DCHECK_EQ(Operation::kFetch, op->operation());
  op->VerifyCompletions(1);  // ReadSmallBufferComplete ran.
  // Fail the request if either read failed.
  if (rv != op->large_buffer()->size() || !op->small_buffer()) {
    op->TakeReadCallback().Run(base::Time(), mojo_base::BigBuffer());
    // Doom this entry since it is inaccessible.
    DoomEntry(op);
  } else {
    base::Time response_time;
    uint32_t data_size = 0;
    ReadSmallDataHeader(op->small_buffer(), &response_time, &data_size);
    if (data_size <= kSmallDataLimit) {
      // Small data, copy the data from the small buffer.
      DCHECK_EQ(0, op->large_buffer()->size());
      mojo_base::BigBuffer data(data_size);
      memcpy(data.data(), op->small_buffer()->data() + kHeaderSizeInBytes,
             data_size);
      op->TakeReadCallback().Run(response_time, std::move(data));
    } else {
      op->TakeReadCallback().Run(response_time,
                                 op->large_buffer()->TakeBuffer());
    }
  }
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::DeleteEntryImpl(PendingOperation* op) {
  DCHECK(op->operation() == Operation::kDelete);
  DoomEntry(op);
  CloseOperationAndIssueNext(op);
}

void GeneratedCodeCache::DoomEntry(PendingOperation* op) {
  // Write, Fetch, and Delete may all doom an entry.
  DCHECK_NE(Operation::kGetBackend, op->operation());
  // Entries shouldn't be doomed if the backend hasn't been initialized.
  DCHECK_EQ(kInitialized, backend_state_);
  CollectStatistics(CacheEntryStatus::kClear);
  backend_->DoomEntry(op->key(), net::LOWEST, net::CompletionOnceCallback());
}

void GeneratedCodeCache::IssueNextOperation(const std::string& key) {
  auto it = active_entries_map_.find(key);
  if (it == active_entries_map_.end())
    return;

  DCHECK(!it->second.empty());
  IssueOperation(it->second.front().get());
}

void GeneratedCodeCache::CloseOperationAndIssueNext(PendingOperation* op) {
  // Dequeue op, keeping it alive long enough to issue another op.
  std::unique_ptr<PendingOperation> keep_alive = DequeueOperation(op);
  IssueNextOperation(op->key());
}

void GeneratedCodeCache::EnqueueOperationAndIssueIfNext(
    std::unique_ptr<PendingOperation> op) {
  // GetBackend ops have no key and shouldn't be enqueued here.
  DCHECK_NE(Operation::kGetBackend, op->operation());
  auto it = active_entries_map_.find(op->key());
  bool can_issue = false;
  if (it == active_entries_map_.end()) {
    it = active_entries_map_.emplace(op->key(), PendingOperationQueue()).first;
    can_issue = true;
  }
  const std::string& key = op->key();
  it->second.emplace(std::move(op));
  if (can_issue)
    IssueNextOperation(key);
}

std::unique_ptr<GeneratedCodeCache::PendingOperation>
GeneratedCodeCache::DequeueOperation(PendingOperation* op) {
  auto it = active_entries_map_.find(op->key());
  DCHECK(it != active_entries_map_.end());
  DCHECK(!it->second.empty());
  std::unique_ptr<PendingOperation> result = std::move(it->second.front());
  // |op| should be at the front.
  DCHECK_EQ(op, result.get());
  it->second.pop();
  // Delete the queue if it becomes empty.
  if (it->second.empty()) {
    active_entries_map_.erase(it);
  }
  return result;
}

void GeneratedCodeCache::DoPendingGetBackend(PendingOperation* op) {
  // |op| is kept alive in |IssuePendingOperations| for the duration of this
  // call. We shouldn't access |op| after returning from this function.
  DCHECK_EQ(kGetBackend, op->operation());
  if (backend_state_ == kInitialized) {
    op->TakeBackendCallback().Run(backend_.get());
  } else {
    DCHECK_EQ(backend_state_, kFailed);
    op->TakeBackendCallback().Run(nullptr);
  }
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
