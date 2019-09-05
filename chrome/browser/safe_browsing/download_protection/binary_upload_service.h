// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_

#include <memory>
#include <unordered_map>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/download_protection/multipart_uploader.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class BinaryUploadService {
 public:
  BinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile);

  // This constructor is useful in tests, if you want to keep a reference to the
  // service's |binary_fcm_service_|.
  BinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<BinaryFCMService> binary_fcm_service);
  ~BinaryUploadService();

  enum class Result {
    // Unknown result.
    UNKNOWN = 0,

    // The request succeeded.
    SUCCESS = 1,

    // The upload failed, for an unspecified reason.
    UPLOAD_FAILURE = 2,

    // The upload succeeded, but a response was not received before timing out.
    TIMEOUT = 3,

    // The file was too large to upload.
    FILE_TOO_LARGE = 4,

    // The BinaryUploadService failed to get an InstanceID token.
    FAILED_TO_GET_TOKEN = 5,
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
    virtual ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;

    // Asynchronously returns the file contents to upload.
    // TODO(drubery): This could allocate up to 50MB of memory for a large file
    // upload. We should see how often that causes errors, and possibly
    // implement some sort of streaming interface so we don't use so much
    // memory.
    virtual void GetFileContents(
        base::OnceCallback<void(const std::string&)> callback) = 0;

    // Returns the content size.
    virtual size_t GetFileSize() = 0;

    // Returns the metadata to upload, as a DeepScanningClientRequest.
    const DeepScanningClientRequest& deep_scanning_request() const {
      return deep_scanning_request_;
    }

    // Methods for modifying the DeepScanningClientRequest.
    void set_request_dlp_scan(DlpDeepScanningClientRequest dlp_request);
    void set_request_malware_scan(
        MalwareDeepScanningClientRequest malware_request);
    void set_fcm_token(const std::string& token);
    void set_dm_token(const std::string& token);
    void set_request_token(const std::string& token);

    // Finish the request, with the given |result| and |response| from the
    // server.
    void FinishRequest(Result result, DeepScanningClientResponse response);

   private:
    DeepScanningClientRequest deep_scanning_request_;
    Callback callback_;
  };

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling |request|'s |callback|. This must be
  // called on the UI thread.
  void UploadForDeepScanning(std::unique_ptr<Request> request);

  // Returns whether a download should be blocked based on file size alone. It
  // checks the enterprise policy BlockLargeFileTransfer to decide this.
  static bool ShouldBlockFileSize(size_t file_size);

 private:
  friend class BinaryUploadServiceTest;

  void OnGetInstanceID(Request* request, const std::string& token);

  void OnGetFileContents(Request* request, const std::string& file_contents);

  void OnUploadComplete(Request* request,
                        bool success,
                        const std::string& response_data);

  void OnGetResponse(Request* request, DeepScanningClientResponse response);

  void MaybeFinishRequest(Request* request);

  void OnTimeout(Request* request);

  void FinishRequest(Request* request,
                     Result result,
                     DeepScanningClientResponse response);

  bool IsActive(Request* request);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;

  // Resources associated with an in-progress request.
  base::flat_map<Request*, std::unique_ptr<Request>> active_requests_;
  base::flat_map<Request*, std::unique_ptr<base::OneShotTimer>> active_timers_;
  base::flat_map<Request*, std::unique_ptr<MultipartUploadRequest>>
      active_uploads_;
  base::flat_map<Request*, std::string> active_tokens_;
  base::flat_map<Request*, std::unique_ptr<MalwareDeepScanningVerdict>>
      received_malware_verdicts_;
  base::flat_map<Request*, std::unique_ptr<DlpDeepScanningVerdict>>
      received_dlp_verdicts_;

  base::WeakPtrFactory<BinaryUploadService> weakptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_BINARY_UPLOAD_SERVICE_H_
