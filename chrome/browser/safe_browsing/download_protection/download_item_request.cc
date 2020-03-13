// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"

#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

std::string GetFileContentsBlocking(base::FilePath path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return "";

  int64_t file_size = file.GetLength();
  if (static_cast<size_t>(file_size) > BinaryUploadService::kMaxUploadSizeBytes)
    return "";

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
                                         bool read_immediately,
                                         BinaryUploadService::Callback callback)
    : Request(std::move(callback)), item_(item), weakptr_factory_(this) {
  if (read_immediately)
    ReadFile();

  item_->AddObserver(this);
}

DownloadItemRequest::~DownloadItemRequest() {
  if (item_ != nullptr)
    item_->RemoveObserver(this);
}

void DownloadItemRequest::GetRequestData(DataCallback callback) {
  if (item_ == nullptr) {
    std::move(callback).Run(BinaryUploadService::Result::UNKNOWN, Data());
    return;
  }

  if (item_ && static_cast<size_t>(item_->GetTotalBytes()) >
                   BinaryUploadService::kMaxUploadSizeBytes) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       BinaryUploadService::Result::FILE_TOO_LARGE, Data()));
    return;
  }

  bool malware = deep_scanning_request().has_malware_scan_request();
  bool dlp = deep_scanning_request().has_dlp_scan_request();
  if (item_ && (malware || dlp) &&
      !FileTypeSupported(malware, dlp, item_->GetTargetFilePath())) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE,
                       Data()));
    return;
  }

  if (is_data_valid_) {
    RunPendingGetFileContentsCallback(std::move(callback));
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
}

void DownloadItemRequest::RunPendingGetFileContentsCallback(
    DataCallback callback) {
  if (is_data_encrypted_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       BinaryUploadService::Result::FILE_ENCRYPTED, Data()));
    return;
  }

  if (is_data_valid_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BinaryUploadService::Result::SUCCESS, data_));
    return;
  }
}

void DownloadItemRequest::OnDownloadUpdated(download::DownloadItem* download) {
  if (!is_data_valid_ && download == item_ &&
      item_->GetFullPath() == item_->GetTargetFilePath())
    ReadFile();
}

void DownloadItemRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  if (download == item_)
    item_ = nullptr;
}

void DownloadItemRequest::ReadFile() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileContentsBlocking, item_->GetFullPath()),
      base::BindOnce(&DownloadItemRequest::OnGotFileContents,
                     weakptr_factory_.GetWeakPtr()));
}

void DownloadItemRequest::OnGotFileContents(std::string contents) {
  data_.contents = std::move(contents);
  base::FilePath current_path;
  base::FilePath::StringType extension;
  if (item_) {
    current_path = item_->GetFullPath();
    extension = item_->GetTargetFilePath().Extension();
  }

  if (extension == FILE_PATH_LITERAL(".zip")) {
    auto analyzer = base::MakeRefCounted<SandboxedZipAnalyzer>(
        current_path,
        base::BindOnce(&DownloadItemRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr()),
        LaunchFileUtilService());
    analyzer->Start();
  } else if (extension == FILE_PATH_LITERAL(".rar")) {
    auto analyzer = base::MakeRefCounted<SandboxedRarAnalyzer>(
        current_path,
        base::BindOnce(&DownloadItemRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr()),
        LaunchFileUtilService());
    analyzer->Start();
  } else {
    OnCheckedForEncryption(ArchiveAnalyzerResults());
  }
}

void DownloadItemRequest::OnCheckedForEncryption(
    const ArchiveAnalyzerResults& results) {
  is_data_valid_ = true;
  is_data_encrypted_ = std::any_of(
      results.archived_binary.begin(), results.archived_binary.end(),
      [](const auto& binary) { return binary.is_encrypted(); });

  for (auto& callback : pending_callbacks_) {
    RunPendingGetFileContentsCallback(std::move(callback));
  }

  pending_callbacks_.clear();
}

}  // namespace safe_browsing
