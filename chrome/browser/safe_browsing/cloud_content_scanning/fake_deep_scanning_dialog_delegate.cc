// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"

#include <base/callback.h>
#include <base/logging.h>

namespace safe_browsing {

FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : DeepScanningDialogDelegate(web_contents,
                                 std::move(data),
                                 std::move(callback)),
      delete_closure_(delete_closure),
      status_callback_(status_callback),
      dm_token_(std::move(dm_token)) {}

FakeDeepScanningDialogDelegate::~FakeDeepScanningDialogDelegate() {
  if (!delete_closure_.is_null())
    delete_closure_.Run();
}

// static
std::unique_ptr<DeepScanningDialogDelegate>
FakeDeepScanningDialogDelegate::Create(base::RepeatingClosure delete_closure,
                                       StatusCallback status_callback,
                                       std::string dm_token,
                                       content::WebContents* web_contents,
                                       Data data,
                                       CompletionCallback callback) {
  auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
      delete_closure, status_callback, std::move(dm_token), web_contents,
      std::move(data), std::move(callback));
  return ret;
}

void FakeDeepScanningDialogDelegate::Response(
    base::FilePath path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DeepScanningClientResponse response;
  bool status = status_callback_.is_null() ? false : status_callback_.Run(path);
  if (!status) {
    // If the scan should fail, mark both the DLP and malware as failed.
    // for DLP, just add a rule to the triggered list., does not matter what
    // the rule is.  For malware, just set the verdict to MALWARE.
    response.mutable_dlp_scan_verdict()->mutable_triggered_rules()->Add();
    response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::MALWARE);
  }

  if (path.empty()) {
    StringRequestCallback(BinaryUploadService::Result::SUCCESS, response);
  } else {
    FileRequestCallback(path, BinaryUploadService::Result::SUCCESS, response);
  }
}

void FakeDeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                                base::Unretained(this), base::FilePath(),
                                std::move(request)));
}

void FakeDeepScanningDialogDelegate::UploadFileForDeepScanning(
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK(!path.empty());
  DCHECK_EQ(
      DlpDeepScanningClientRequest::FILE_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     base::Unretained(this), path, std::move(request)));
}

bool FakeDeepScanningDialogDelegate::CloseTabModalDialog() {
  return false;
}

}  // namespace safe_browsing
