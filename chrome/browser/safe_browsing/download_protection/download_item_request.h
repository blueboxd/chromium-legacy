// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "components/download/public/common/download_item.h"

namespace safe_browsing {

// This class implements the BinaryUploadService::Request interface for a
// particular DownloadItem. It is neither moveable nor copyable.
class DownloadItemRequest : public BinaryUploadService::Request,
                            download::DownloadItem::Observer {
 public:
  DownloadItemRequest(download::DownloadItem* item,
                      BinaryUploadService::Callback callback);
  ~DownloadItemRequest() override;

  DownloadItemRequest(const DownloadItemRequest&) = delete;
  DownloadItemRequest& operator=(const DownloadItemRequest&) = delete;
  DownloadItemRequest(DownloadItemRequest&&) = delete;
  DownloadItemRequest& operator=(DownloadItemRequest&&) = delete;

  // BinaryUploadService::Request implementation.
  void GetFileContents(
      base::OnceCallback<void(const std::string&)> callback) override;
  size_t GetFileSize() override;

  // download::DownloadItem::Observer implementation.
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  void OnGotFileContents(base::OnceCallback<void(const std::string&)> callback,
                         const std::string& contents);

  // Calls to GetFileContents can be deferred if the download item is not yet
  // renamed to its final location. When ready, this method runs those
  // callbacks.
  void RunPendingGetFileContentsCallbacks();

  // Pointer the download item for upload. This must be accessed only the UI
  // thread. Unowned.
  download::DownloadItem* item_;

  // Whether the download item has been renamed to its final destination yet.
  bool download_item_renamed_;

  // All pending callbacks to GetFileContents before the download item is ready.
  std::vector<base::OnceCallback<void(const std::string&)>> pending_callbacks_;

  base::WeakPtrFactory<DownloadItemRequest> weakptr_factory_;
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_ITEM_REQUEST_H_
