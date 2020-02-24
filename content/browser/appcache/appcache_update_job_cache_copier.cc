// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_job_cache_copier.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_update_metrics_recorder.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "content/browser/appcache/appcache_update_url_loader_request.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {
const int kAppCacheFetchBufferSize = 32768;

void UpdateHttpInfo(net::HttpResponseInfo* response,
                    net::HttpResponseInfo* new_response,
                    AppCacheUpdateMetricsRecorder* metrics) {
  response->headers->Update(*new_response->headers.get());
  response->stale_revalidate_timeout = base::Time();
  bool was_corrupt = false;
  if (response->response_time.is_null() || response->request_time.is_null()) {
    was_corrupt = true;
    // Metrics for corrupt resources are tracked in AppCacheUpdateJob's
    // CanUseExistingResource(), so we don't change anything here to avoid
    // double-counting.
  }
  response->response_time = new_response->response_time;
  response->request_time = new_response->request_time;
  if (was_corrupt && !response->response_time.is_null() &&
      !response->request_time.is_null()) {
    metrics->IncrementExistingCorruptionFixedInUpdate();
  }
  response->network_accessed = new_response->network_accessed;
  response->unused_since_prefetch = new_response->unused_since_prefetch;
  response->restricted_prefetch = new_response->restricted_prefetch;
  response->ssl_info = new_response->ssl_info;
  if (new_response->vary_data.is_valid()) {
    response->vary_data = new_response->vary_data;
  } else if (response->vary_data.is_valid()) {
    // There is a vary header in the stored response but not in the current one.
    // This requires access to the request's HttpRequestInfo which we don't
    // have.  See HttpCache::Transaction::DoUpdateCachedResponse() for more
    // info.
    metrics->IncrementExistingVaryDuring304();
  }
}

}  // namespace

AppCacheUpdateJob::CacheCopier::CacheCopier(
    AppCacheUpdateJob* job,
    const GURL& url,
    const GURL& manifest_url,
    std::unique_ptr<AppCacheUpdateJob::URLFetcher> incoming_fetcher,
    std::unique_ptr<AppCacheResponseWriter> response_writer)
    : job_(job),
      url_(url),
      manifest_url_(manifest_url),
      incoming_fetcher_(std::move(incoming_fetcher)),
      response_writer_(std::move(response_writer)),
      state_(State::kIdle),
      read_data_size_(0) {
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
}

AppCacheUpdateJob::CacheCopier::~CacheCopier() = default;

void AppCacheUpdateJob::CacheCopier::Cancel() {
  state_ = State::kCanceled;
}

void AppCacheUpdateJob::CacheCopier::Run() {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }

  DCHECK_EQ(state_, State::kIdle);
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK(!existing_appcache_response_info_);
  DCHECK(incoming_fetcher_->existing_entry().has_response_id());
  DCHECK(!read_data_response_reader_);
  DCHECK(!read_data_buffer_);
  DCHECK(!merged_http_info_);
  DCHECK(response_writer_);

  incoming_http_info_ = std::make_unique<net::HttpResponseInfo>(
      incoming_fetcher_->request()->GetResponseInfo());

  read_data_response_reader_ = job_->service_->storage()->CreateResponseReader(
      manifest_url_, incoming_fetcher_->existing_entry().response_id());
  read_data_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(kAppCacheFetchBufferSize);

  state_ = State::kAwaitReadInfoComplete;
  job_->service_->storage()->LoadResponseInfo(
      manifest_url_, incoming_fetcher_->existing_entry().response_id(), this);
  // Async continues in |OnResponseInfoLoaded|.
}

void AppCacheUpdateJob::CacheCopier::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }

  DCHECK(response_info);
  DCHECK_EQ(state_, State::kAwaitReadInfoComplete);
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(incoming_fetcher_->existing_entry().response_id(), response_id);
  DCHECK(!existing_appcache_response_info_);
  DCHECK(!merged_http_info_);

  existing_appcache_response_info_ = response_info;

  // Update the cached HttpResponseInfo with the incoming HttpResponseInfo.
  merged_http_info_ = std::make_unique<net::HttpResponseInfo>(
      existing_appcache_response_info_->http_response_info());
  UpdateHttpInfo(merged_http_info_.get(), incoming_http_info_.get(),
                 &job_->update_metrics_);

  // Get ready to write the merged HttpResponseInfo to disk.
  scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(
          std::move(merged_http_info_));

  // Prepare for next state.
  state_ = State::kAwaitWriteInfoComplete;

  // Trigger async WriteInfo() call.
  response_writer_->WriteInfo(
      io_buffer.get(),
      base::BindOnce(&AppCacheUpdateJob::CacheCopier::OnWriteInfoComplete,
                     base::Unretained(this)));
  // Async continues in |OnWriteInfoComplete| with state kOnWriteInfoComplete.
}

void AppCacheUpdateJob::CacheCopier::OnWriteInfoComplete(int result) {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(state_, State::kAwaitWriteInfoComplete);
  DCHECK_GT(result, 0);

  // Prepare for next state.
  result = 0;
  state_ = State::kReadData;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppCacheUpdateJob::CacheCopier::ReadData,
                                base::Unretained(this)));
  // Async continues in |ReadData| with state kReadData.
}

void AppCacheUpdateJob::CacheCopier::ReadData() {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(state_, State::kReadData);
  DCHECK(read_data_response_reader_);
  DCHECK(read_data_buffer_);

  // Prepare for next state.
  state_ = State::kAwaitReadDataComplete;

  read_data_response_reader_->ReadData(
      read_data_buffer_.get(), kAppCacheFetchBufferSize,
      base::BindOnce(&AppCacheUpdateJob::CacheCopier::OnReadDataComplete,
                     base::Unretained(this)));
  // Async continues in |OnReadDataComplete| with state kAwaitReadDataComplete.
}

void AppCacheUpdateJob::CacheCopier::OnReadDataComplete(int result) {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(state_, State::kAwaitReadDataComplete);
  DCHECK_GE(result, 0);

  if (result == 0) {
    // If there's no more to read, then we're done copying data.

    // Prepare for next state.
    state_ = State::kCopyComplete;

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheUpdateJob::CacheCopier::CopyComplete,
                                  base::Unretained(this)));
    // Async continues in |CopyComplete| with state kCopyComplete.
    return;
  }

  read_data_size_ = result;

  // Prepare for next state.
  state_ = State::kAwaitWriteDataComplete;

  // Trigger async WriteData() call.
  response_writer_->WriteData(
      read_data_buffer_.get(), read_data_size_,
      base::BindOnce(&AppCacheUpdateJob::CacheCopier::OnWriteDataComplete,
                     base::Unretained(this)));
  // Async continues in |OnWriteDataComplete| with state
  // kAwaitWriteDataComplete.
}

void AppCacheUpdateJob::CacheCopier::OnWriteDataComplete(int result) {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(state_, State::kAwaitWriteDataComplete);
  DCHECK_GE(result, 0);
  DCHECK_EQ(read_data_size_, result);

  // Prepare for next state.
  read_data_size_ = 0;
  state_ = State::kReadData;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppCacheUpdateJob::CacheCopier::ReadData,
                                base::Unretained(this)));
  // Async continues in |ReadData| with state kReadData.
}

void AppCacheUpdateJob::CacheCopier::CopyComplete() {
  if (job_->IsFinished()) {
    Cancel();
    return;
  }
  DCHECK_EQ(job_->internal_state_, AppCacheUpdateJobState::DOWNLOADING);
  DCHECK_EQ(state_, State::kCopyComplete);
  // |job_| owns |this|, so calling ContinueHandleResourceFetchCompleted
  // will result in |this| being deleted by |job_|.  Post this as a task to
  // allow control flow here to exit first.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AppCacheUpdateJob::ContinueHandleResourceFetchCompleted,
                     base::Unretained(job_), url_,
                     base::Owned(std::move(incoming_fetcher_))));
}

}  // namespace content
