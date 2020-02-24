// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_cache_test_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {
const int kAppCacheFetchBufferSize = 32768;
}  // namespace

AppCacheCacheTestHelper::CacheEntry::CacheEntry(
    int types,
    std::string expect_if_modified_since,
    std::string expect_if_none_match,
    bool headers_allowed,
    std::unique_ptr<net::HttpResponseInfo> response_info,
    const std::string& body)
    : types(types),
      expect_if_modified_since(expect_if_modified_since),
      expect_if_none_match(expect_if_none_match),
      headers_allowed(headers_allowed),
      response_info(std::move(response_info)),
      body(body) {}

AppCacheCacheTestHelper::CacheEntry::~CacheEntry() = default;

// static
void AppCacheCacheTestHelper::AddCacheEntry(
    CacheEntries* cache_entries,
    const GURL& url,
    int types,
    std::string expect_if_modified_since,
    std::string expect_if_none_match,
    bool headers_allowed,
    std::unique_ptr<net::HttpResponseInfo> response_info,
    const std::string& body) {
  cache_entries->emplace(
      url, std::make_unique<AppCacheCacheTestHelper::CacheEntry>(
               types, expect_if_modified_since, expect_if_none_match,
               headers_allowed, std::move(response_info), body));
}

AppCacheCacheTestHelper::AppCacheCacheTestHelper(
    const MockAppCacheService* service,
    const GURL& manifest_url,
    AppCache* const cache,
    CacheEntries cache_entries,
    base::OnceCallback<void(int)> post_write_callback)
    : service_(service),
      manifest_url_(manifest_url),
      cache_(cache),
      cache_entries_(std::move(cache_entries)),
      state_(State::kIdle),
      post_write_callback_(std::move(post_write_callback)) {}

AppCacheCacheTestHelper::~AppCacheCacheTestHelper() = default;

void AppCacheCacheTestHelper::PrepareForRead(
    AppCache* read_cache,
    base::OnceClosure post_read_callback) {
  read_cache_ = read_cache;
  post_read_callback_ = std::move(post_read_callback);
}

void AppCacheCacheTestHelper::Read() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kReadInfo;
  read_it_ = read_cache_->entries().begin();
  read_cache_entries_.clear();
  AsyncRead(0);
}

void AppCacheCacheTestHelper::Write() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kWriteInfo;
  write_it_ = cache_entries_.begin();
  AsyncWrite(0);
}

void AppCacheCacheTestHelper::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(response_info);
  DCHECK_EQ(read_entry_response_id_, response_id);

  read_info_response_info_.reset();
  read_info_response_info_ = response_info;
  AsyncRead(0);
}

void AppCacheCacheTestHelper::AsyncRead(int result) {
  DCHECK(state_ == State::kReadInfo || state_ == State::kReadData);
  DCHECK_GE(result, 0);
  if (read_it_ == read_cache_->entries().end()) {
    state_ = State::kIdle;
    std::move(post_read_callback_).Run();
    return;
  }
  switch (state_) {
    case State::kReadInfo: {
      if (!read_info_response_info_) {
        AppCacheEntry* entry = read_cache_->GetEntry(read_it_->first);
        DCHECK(entry);
        read_entry_response_id_ = entry->response_id();
        service_->storage()->LoadResponseInfo(manifest_url_,
                                              read_entry_response_id_, this);
      } else {
        // Result is in |read_info_response_info_|.  Keep it there for now.
        // Will be passed along after data is read.

        // Prepare for next state.
        state_ = State::kReadData;

        // Trigger data read for |read_entry_response_id_|.
        read_data_response_reader_ = service_->storage()->CreateResponseReader(
            read_it_->first, read_entry_response_id_);
        read_data_buffer_ =
            base::MakeRefCounted<net::IOBuffer>(kAppCacheFetchBufferSize);
        read_data_response_reader_->ReadData(
            read_data_buffer_.get(), kAppCacheFetchBufferSize,
            base::BindOnce(&AppCacheCacheTestHelper::AsyncRead,
                           base::Unretained(this)));
      }
    } break;
    case State::kReadData: {
      if (result > 0) {
        read_data_loaded_data_.append(read_data_buffer_->data(), result);
        read_data_response_reader_->ReadData(
            read_data_buffer_.get(), kAppCacheFetchBufferSize,
            base::BindOnce(&AppCacheCacheTestHelper::AsyncRead,
                           base::Unretained(this)));
      } else {
        // Result is in |read_data_loaded_data_|.

        std::unique_ptr<net::HttpResponseInfo> http_response_info =
            std::make_unique<net::HttpResponseInfo>(
                read_info_response_info_->http_response_info());
        AppCacheCacheTestHelper::AddCacheEntry(
            &read_cache_entries_, read_it_->first, AppCacheEntry::EXPLICIT,
            /*expect_if_modified_since=*/std::string(),
            /*expect_if_none_match=*/std::string(), /*headers_allowed=*/false,
            std::move(http_response_info), read_data_loaded_data_);

        // Reset after read.
        read_info_response_info_.reset();
        read_entry_response_id_ = 0;
        read_data_buffer_.reset();
        read_data_loaded_data_ = "";
        read_data_response_reader_.reset();

        // Prepare for next state.
        state_ = State::kReadInfo;
        read_it_++;  // move to next entry
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&AppCacheCacheTestHelper::AsyncRead,
                                      base::Unretained(this), 0));
      }
    } break;
    default:
      NOTREACHED();
      break;
  }
}

void AppCacheCacheTestHelper::AsyncWrite(int result) {
  DCHECK(state_ == State::kWriteInfo || state_ == State::kWriteData);
  DCHECK_GE(result, 0);
  if (write_it_ == cache_entries_.end()) {
    state_ = State::kIdle;
    std::move(post_write_callback_).Run(1);
    return;
  }
  switch (state_) {
    case State::kWriteInfo: {
      // Prepare for info write.
      response_writer_.reset();
      response_writer_ =
          service_->storage()->CreateResponseWriter(manifest_url_);
      AppCacheEntry* entry = cache_->GetEntry(write_it_->first);
      if (entry) {
        entry->add_types(write_it_->second->types);
        entry->set_response_id(response_writer_->response_id());
      } else {
        cache_->AddEntry(write_it_->first,
                         AppCacheEntry(write_it_->second->types,
                                       response_writer_->response_id()));
      }
      // Copy |response_info| so later calls can access the original response
      // info.
      std::unique_ptr<net::HttpResponseInfo> http_response_info =
          std::make_unique<net::HttpResponseInfo>(
              *(write_it_->second->response_info));
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
          base::MakeRefCounted<HttpResponseInfoIOBuffer>(
              std::move(http_response_info));

      // Prepare for next state.
      state_ = State::kWriteData;

      // Trigger async WriteInfo() call.
      response_writer_->WriteInfo(
          io_buffer.get(), base::BindOnce(&AppCacheCacheTestHelper::AsyncWrite,
                                          base::Unretained(this)));
    } break;
    case State::kWriteData: {
      // Prepare for data write.
      std::string body = write_it_->second->body;
      scoped_refptr<net::StringIOBuffer> io_buffer =
          base::MakeRefCounted<net::StringIOBuffer>(body);

      // Prepare for next state.
      state_ = State::kWriteInfo;
      write_it_++;  // move to next entry

      // Trigger async WriteData() call.
      response_writer_->WriteData(
          io_buffer.get(), body.length(),
          base::BindOnce(&AppCacheCacheTestHelper::AsyncWrite,
                         base::Unretained(this)));
    } break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
