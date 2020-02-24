// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_CACHE_COPIER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_CACHE_COPIER_H_

#include <stdint.h>

#include <memory>

#include "content/browser/appcache/appcache_storage.h"
#include "content/browser/appcache/appcache_update_job.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace net {

class IOBuffer;
class HttpResponseInfo;

}  // namespace net

namespace content {

class AppCacheResponseInfo;
class AppCacheResponseReader;
class AppCacheResponseWriter;

// Helper class to read cache info from and write cache info to disk.
class AppCacheUpdateJob::CacheCopier : public AppCacheStorage::Delegate {
 public:
  CacheCopier(AppCacheUpdateJob* job,
              const GURL& url,
              const GURL& manifest_url,
              std::unique_ptr<AppCacheUpdateJob::URLFetcher> incoming_fetcher,
              std::unique_ptr<AppCacheResponseWriter> response_writer);
  ~CacheCopier() override;

  void Run();

  AppCacheResponseWriter* response_writer() { return response_writer_.get(); }

 private:
  // Cache copies follow this flow:
  //
  // 1. Run() is called, state is kIdle, basic objects are set up to support
  //    reading and writing.  State moves to kAwaitReadInfoComplete and
  //    ResponseInfoLoad() is triggered.
  //
  // 2. ResponseInfoLoad() completes async and calls OnResponseInfoLoaded(),
  //    which verifies state is kAwaitReadInfoComplete.  The cached response
  //    info is updated with the incoming 304 response.  Basic objects are
  //    prepared to write the new response info to a new response id.  State
  //    moves to kAwaitWriteInfoComplete and WriteInfo() is triggered.
  //
  // 3. WriteInfo() completes async and calls OnWriteInfoComplete(), which
  //    verifies state kAwaitWriteComplete, that result (ie. number of
  //    bytes written) is greater than 0, sets state kReadData and async calls
  //    ReadData().
  //
  // 4. ReadData() handles state kReadData, which verifies that basic objects
  //    exist to read a chunk of data from the cached response id's data,
  //    state moves to kAwaitReadDataComplete, and ReadData() is triggered.
  //
  // 5. ReadData() completes async and calls OnReadDataComplete(), which detects
  //    state kAwaitReadDataComplete.
  //
  //      If result (ie. the number of bytes read) is 0, state becomes
  //      kFinalize and a task is posted to trigger CopyComplete().
  //
  //      If result is greater than 0, state becomes kAwaitWriteDataComplete
  //      and WriteData() is called.
  //
  // 6. WriteData() completes async and calls OnWriteDataComplete(), which
  //    verifies state kAwaitWriteDataComplete.  State becomes kReadData and we
  //    post a task to call ReadData() again to repeat from step 4.
  //
  // 7. CopyComplete() can be called which verifies state kCopyComplete.  From
  //    here, we post a task to let the update job know we've completed the copy
  //    and the resource fetch completed handling can continue.
  //
  // 8. The posted task in update job deletes the cache copy instance.
  //
  // 9. If the update job has been canceled, then we exit handling our copy
  //    and set state to kCanceled.
  enum class State {
    kIdle,
    kAwaitReadInfoComplete,
    kAwaitWriteInfoComplete,
    kReadData,
    kAwaitReadDataComplete,
    kAwaitWriteDataComplete,
    kCopyComplete,
    kCanceled,
  };

  // Methods for AppCacheStorage::Delegate.
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;

  void Cancel();
  void OnWriteInfoComplete(int result);
  void ReadData();
  void OnReadDataComplete(int result);
  void OnWriteDataComplete(int result);
  void CopyComplete();

  AppCacheUpdateJob* const job_;
  const GURL url_;
  const GURL manifest_url_;
  std::unique_ptr<AppCacheUpdateJob::URLFetcher> incoming_fetcher_;
  std::unique_ptr<AppCacheResponseWriter> response_writer_;
  State state_;

  std::unique_ptr<net::HttpResponseInfo> incoming_http_info_;
  scoped_refptr<AppCacheResponseInfo> existing_appcache_response_info_;
  std::unique_ptr<net::HttpResponseInfo> merged_http_info_;

  std::unique_ptr<AppCacheResponseReader> read_data_response_reader_;
  scoped_refptr<net::IOBuffer> read_data_buffer_;
  int read_data_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_CACHE_COPIER_H_
