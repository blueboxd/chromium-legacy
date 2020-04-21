// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_SOURCE_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_SOURCE_REQUEST_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"

namespace safe_browsing {

// A BinaryUploadService::Request implementation that gets the data to scan
// from the contents of a file. It caches the results so that future calls to
// GetRequestData will return quickly.
class FileSourceRequest : public BinaryUploadService::Request {
 public:
  FileSourceRequest(base::FilePath path,
                    BinaryUploadService::Callback callback);
  FileSourceRequest(const FileSourceRequest&) = delete;
  FileSourceRequest& operator=(const FileSourceRequest&) = delete;
  ~FileSourceRequest() override;

  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

 private:
  void OnGotFileData(
      DataCallback callback,
      std::pair<BinaryUploadService::Result, Data> result_and_data);

  void OnCheckedForEncryption(DataCallback callback,
                              Data data,
                              const ArchiveAnalyzerResults& analyzer_result);

  void CacheResultAndData(BinaryUploadService::Result result, Data data);

  bool has_cached_result_;
  BinaryUploadService::Result cached_result_;
  Data cached_data_;

  base::FilePath path_;
  base::WeakPtrFactory<FileSourceRequest> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_SOURCE_REQUEST_H_
