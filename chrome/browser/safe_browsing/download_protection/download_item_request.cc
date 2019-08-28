// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

std::string GetFileContentsBlocking(base::FilePath path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return "";

  int64_t file_size = file.GetLength();
  std::string contents;
  contents.resize(file_size);

  int64_t bytes_read = 0;
  while (bytes_read < file_size) {
    int64_t bytes_currently_read =
        file.ReadAtCurrentPos(&contents[bytes_read], file_size - bytes_read);
    if (bytes_currently_read == -1)
      return "";

    bytes_read += bytes_currently_read;
  }

  return contents;
}

}  // namespace

DownloadItemRequest::DownloadItemRequest(download::DownloadItem* item,
                                         BinaryUploadService::Callback callback)
    : Request(std::move(callback)),
      item_(item),
      download_item_renamed_(false),
      weakptr_factory_(this) {
  item_->AddObserver(this);
}

DownloadItemRequest::~DownloadItemRequest() {
  if (item_ != nullptr)
    item_->RemoveObserver(this);
}

void DownloadItemRequest::GetFileContents(
    base::OnceCallback<void(const std::string&)> callback) {
  if (item_ == nullptr) {
    std::move(callback).Run("");
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  if (download_item_renamed_)
    RunPendingGetFileContentsCallbacks();
}

void DownloadItemRequest::RunPendingGetFileContentsCallbacks() {
  for (auto it = pending_callbacks_.begin(); it != pending_callbacks_.end();
       it++) {
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
         base::MayBlock()},
        base::BindOnce(&GetFileContentsBlocking, item_->GetFullPath()),
        base::BindOnce(&DownloadItemRequest::OnGotFileContents,
                       weakptr_factory_.GetWeakPtr(), std::move(*it)));
  }

  pending_callbacks_.clear();
}

size_t DownloadItemRequest::GetFileSize() {
  return item_ == nullptr ? 0 : item_->GetTotalBytes();
}

void DownloadItemRequest::OnDownloadUpdated(download::DownloadItem* download) {
  if (download == item_ && item_->GetFullPath() == item_->GetTargetFilePath()) {
    download_item_renamed_ = true;
    RunPendingGetFileContentsCallbacks();
  }
}

void DownloadItemRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  if (download == item_)
    item_ = nullptr;
}

void DownloadItemRequest::OnGotFileContents(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& contents) {
  std::move(callback).Run(contents);
}

}  // namespace safe_browsing
