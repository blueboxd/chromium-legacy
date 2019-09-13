// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

class CheckClientDownloadRequest : public CheckClientDownloadRequestBase,
                                   public download::DownloadItem::Observer {
 public:
  CheckClientDownloadRequest(
      download::DownloadItem* item,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~CheckClientDownloadRequest() override;

  void OnDownloadDestroyed(download::DownloadItem* download) override;
  static bool IsSupportedDownload(const download::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason,
                                  ClientDownloadRequest::DownloadType* type);

 private:
  // CheckClientDownloadRequestBase overrides:
  bool IsSupportedDownload(DownloadCheckResultReason* reason,
                           ClientDownloadRequest::DownloadType* type) override;
  content::BrowserContext* GetBrowserContext() override;
  bool IsCancelled() override;
  void PopulateRequest(ClientDownloadRequest* request) override;
  base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() override;

  void NotifySendRequest(const ClientDownloadRequest* request) override;
  void SetDownloadPingToken(const std::string& token) override;
  void MaybeStorePingsForDownload(DownloadCheckResult result,
                                  bool upload_requested,
                                  const std::string& request_data,
                                  const std::string& response_body) override;
  void MaybeUploadBinary(DownloadCheckResultReason reason) override;
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;

  bool ShouldUploadForDlpScan();
  bool ShouldUploadForMalwareScan(DownloadCheckResultReason reason);

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  download::DownloadItem* item_;

  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

// Function that can be called from BinaryUploadService::Callback to report
// verdicts.  Only successful results are reported, assuming that reporting
// is turned on by enterprise policy.
void MaybeReportDownloadDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    BinaryUploadService::Result result,
    DeepScanningClientResponse response);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
