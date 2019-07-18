// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_

#include "base/callback.h"
#include "components/safe_browsing/proto/webprotect.pb.h"

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class BinaryUploadService {
 public:
  enum class Result {
    // The request succeeded.
    SUCCESS = 0,

    // The upload failed, for an unspecified reason.
    UPLOAD_FAILURE = 1,

    // The upload succeeded, but a response was not received before timing out.
    TIMEOUT = 2,

    // The file was too large to upload.
    FILE_TOO_LARGE = 3,
  };

  // Callbacks used to pass along the results of scanning. The response protos
  // will only be populated if the result is SUCCESS.
  using Callback = base::OnceCallback<void(Result, DeepScanningClientResponse)>;

  // A class to encapsulate the a request for upload. This class will provide
  // all the functionality needed to generate a DeepScanningRequest, and
  // subclasses will provide different sources of data to upload (e.g. file or
  // string).
  class Request {
   public:
    // |callback| will run on the UI thread.
    explicit Request(Callback callback);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Returns the file contents to upload.
    // TODO(drubery): This could allocate up to 50MB of memory for a large file
    // upload. We should see how often that causes errors, and possibly
    // implement some sort of streaming interface so we don't use so much
    // memory.
    virtual std::string GetFileContents() = 0;

    // Returns the metadata to upload, as a DeepScanningClientRequest.
    const DeepScanningClientRequest& request() const {
      return deep_scanning_request_;
    }

    // Methods for modifying the DeepScanningClientRequest.
    void set_request_dlp_scan(bool request);
    void set_request_malware_scan(bool request);
    void set_download_token(const std::string& token);

   private:
    DeepScanningClientRequest deep_scanning_request_;
  };

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling |request|'s |callback|. This must be
  // called on the UI thread.
  void UploadForDeepScanning(Request request);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_
