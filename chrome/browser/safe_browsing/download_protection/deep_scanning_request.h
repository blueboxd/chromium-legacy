// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"

namespace download {
class DownloadItem;
}

namespace safe_browsing {

class DownloadProtectionService;

// This class encapsulates the process of uploading a file to Safe Browsing for
// deep scanning and reporting the result.
class DeepScanningRequest : public download::DownloadItem::Observer {
 public:
  // Enum representing the trigger of the scan request.
  enum class DeepScanTrigger {
    // The trigger is unknown.
    TRIGGER_UNKNOWN,

    // The trigger is the prompt in the download shelf, shown for Advanced
    // Protection users.
    TRIGGER_APP_PROMPT,

    // The trigger is the enterprise policy.
    TRIGGER_POLICY,
  };

  // Enum representing the possible scans for a deep scanning request.
  enum class DeepScanType {
    // Scan for malware
    SCAN_MALWARE,

    // Scan for DLP policy violations
    SCAN_DLP,
  };

  // Checks the current policies to determine whether files must be uploaded by
  // policy.
  static bool ShouldUploadItemByPolicy(download::DownloadItem* item);

  // Returns all scans supported.
  static std::vector<DeepScanType> AllScans();

  // Scan the given |item|, with the given |trigger|. The result of the scanning
  // will be provided through |callback|. Take references to the owning
  // |download_service| and the |binary_upload_service| to upload to.
  DeepScanningRequest(download::DownloadItem* item,
                      DeepScanTrigger trigger,
                      CheckDownloadRepeatingCallback callback,
                      DownloadProtectionService* download_service);

  // Same as the previous constructor, but only allowing the scans listed in
  // |allowed_scans| to run.
  DeepScanningRequest(download::DownloadItem* item,
                      DeepScanTrigger trigger,
                      CheckDownloadRepeatingCallback callback,
                      DownloadProtectionService* download_service,
                      std::vector<DeepScanType> allowed_scans);

  ~DeepScanningRequest() override;

  // Begin the deep scanning request. This must be called on the UI thread.
  void Start();

 private:
  // Callback for when |binary_upload_service_| finishes uploading.
  void OnScanComplete(BinaryUploadService::Result result,
                      DeepScanningClientResponse response);

  // Finishes the request, providing the result through |callback_| and
  // notifying |download_service_|.
  void FinishRequest(DownloadCheckResult result);

  // Callback when |item_| is destroyed.
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Called to attempt to show the modal dialog for scan failure. Returns
  // whether the dialog was successfully shown.
  bool MaybeShowDeepScanFailureModalDialog(base::OnceClosure accept_callback,
                                           base::OnceClosure cancel_callback,
                                           base::OnceClosure open_now_callback);

  // Called to open the download. This is triggered by the timeout modal dialog.
  void OpenDownload();

  // Whether the given |scan| is in the list of |allowed_scans_|.
  bool ScanIsAllowed(DeepScanType scan);

  // The download item to scan. This is unowned, and could become nullptr if the
  // download is destroyed.
  download::DownloadItem* item_;

  // The reason for deep scanning.
  DeepScanTrigger trigger_;

  // The callback to provide the scan result to.
  CheckDownloadRepeatingCallback callback_;

  // The download protection service that initiated this upload. The
  // |download_service_| owns this class.
  DownloadProtectionService* download_service_;

  // The scans allowed to be performed.
  std::vector<DeepScanType> allowed_scans_;

  // The time when uploading starts.
  base::TimeTicks upload_start_time_;

  base::WeakPtrFactory<DeepScanningRequest> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
