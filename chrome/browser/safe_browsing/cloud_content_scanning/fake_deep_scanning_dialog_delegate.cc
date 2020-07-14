// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"

#include <base/callback.h>
#include <base/logging.h>
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

base::TimeDelta response_delay = base::TimeDelta::FromSeconds(0);

}  // namespace

BinaryUploadService::Result FakeDeepScanningDialogDelegate::result_ =
    BinaryUploadService::Result::SUCCESS;

FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : DeepScanningDialogDelegate(web_contents,
                                 std::move(data),
                                 std::move(callback),
                                 DeepScanAccessPoint::UPLOAD),
      delete_closure_(delete_closure),
      status_callback_(status_callback),
      encryption_callback_(encryption_callback),
      dm_token_(std::move(dm_token)),
      use_legacy_protos_(true) {}

FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate(
    base::RepeatingClosure delete_closure,
    ContentAnalysisStatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : DeepScanningDialogDelegate(web_contents,
                                 std::move(data),
                                 std::move(callback),
                                 DeepScanAccessPoint::UPLOAD),
      delete_closure_(delete_closure),
      content_analysis_status_callback_(status_callback),
      encryption_callback_(encryption_callback),
      dm_token_(std::move(dm_token)),
      use_legacy_protos_(false) {}

FakeDeepScanningDialogDelegate::~FakeDeepScanningDialogDelegate() {
  if (!delete_closure_.is_null())
    delete_closure_.Run();
}

// static
void FakeDeepScanningDialogDelegate::SetResponseResult(
    BinaryUploadService::Result result) {
  result_ = result;
}

// static
std::unique_ptr<DeepScanningDialogDelegate>
FakeDeepScanningDialogDelegate::Create(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback) {
  auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
      delete_closure, status_callback, encryption_callback, std::move(dm_token),
      web_contents, std::move(data), std::move(callback));
  return ret;
}

// static
std::unique_ptr<DeepScanningDialogDelegate>
FakeDeepScanningDialogDelegate::CreateForConnectors(
    base::RepeatingClosure delete_closure,
    ContentAnalysisStatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback) {
  auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
      delete_closure, status_callback, encryption_callback, std::move(dm_token),
      web_contents, std::move(data), std::move(callback));
  return ret;
}

// static
void FakeDeepScanningDialogDelegate::SetResponseDelay(base::TimeDelta delay) {
  response_delay = delay;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::SuccessfulResponse(
    bool include_dlp,
    bool include_malware) {
  DeepScanningClientResponse response;
  if (include_dlp) {
    response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
  }
  if (include_malware) {
    response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::CLEAN);
  }

  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeDeepScanningDialogDelegate::SuccessfulResponse(
    const std::set<std::string>& tags) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  for (const std::string& tag : tags)
    result->set_tag(tag);

  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::MalwareResponse(
    MalwareDeepScanningVerdict::Verdict verdict) {
  DeepScanningClientResponse response;
  response.mutable_malware_scan_verdict()->set_verdict(verdict);
  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeDeepScanningDialogDelegate::MalwareResponse(
    enterprise_connectors::TriggeredRule::Action action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("malware");

  auto* rule = result->add_triggered_rules();
  rule->set_action(action);

  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::DlpResponse(
    DlpDeepScanningVerdict::Status status,
    const std::string& rule_name,
    DlpDeepScanningVerdict::TriggeredRule::Action action) {
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict()->set_status(status);

  if (!rule_name.empty()) {
    DlpDeepScanningVerdict::TriggeredRule* rule =
        response.mutable_dlp_scan_verdict()->add_triggered_rules();
    rule->set_rule_name(rule_name);
    rule->set_action(action);
  }

  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeDeepScanningDialogDelegate::DlpResponse(
    enterprise_connectors::ContentAnalysisResponse::Result::Status status,
    const std::string& rule_name,
    enterprise_connectors::TriggeredRule::Action action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(status);
  result->set_tag("dlp");

  auto* rule = result->add_triggered_rules();
  rule->set_rule_name(rule_name);
  rule->set_action(action);

  return response;
}

// static
DeepScanningClientResponse
FakeDeepScanningDialogDelegate::MalwareAndDlpResponse(
    MalwareDeepScanningVerdict::Verdict verdict,
    DlpDeepScanningVerdict::Status status,
    const std::string& rule_name,
    DlpDeepScanningVerdict::TriggeredRule::Action action) {
  DeepScanningClientResponse response;
  *response.mutable_malware_scan_verdict() =
      FakeDeepScanningDialogDelegate::MalwareResponse(verdict)
          .malware_scan_verdict();
  *response.mutable_dlp_scan_verdict() =
      FakeDeepScanningDialogDelegate::DlpResponse(status, rule_name, action)
          .dlp_scan_verdict();
  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse MalwareAndDlpResponse(
    enterprise_connectors::TriggeredRule::Action malware_action,
    enterprise_connectors::ContentAnalysisResponse::Result::Status dlp_status,
    const std::string& dlp_rule_name,
    enterprise_connectors::TriggeredRule::Action dlp_action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* malware_result = response.mutable_results()->Add();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(malware_action);

  auto* dlp_result = response.mutable_results()->Add();
  dlp_result->set_status(dlp_status);
  dlp_result->set_tag("dlp");
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_rule_name(dlp_rule_name);
  dlp_rule->set_action(dlp_action);

  return response;
}

void FakeDeepScanningDialogDelegate::Response(
    base::FilePath path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  if (use_legacy_protos()) {
    DeepScanningClientResponse response =
        (status_callback_.is_null() ||
         result_ != BinaryUploadService::Result::SUCCESS)
            ? DeepScanningClientResponse()
            : status_callback_.Run(path);
    if (path.empty())
      StringRequestCallback(result_, response);
    else
      FileRequestCallback(path, result_, response);
  } else {
    auto response = (content_analysis_status_callback_.is_null() ||
                     result_ != BinaryUploadService::Result::SUCCESS)
                        ? enterprise_connectors::ContentAnalysisResponse()
                        : content_analysis_status_callback_.Run(path);
    if (path.empty())
      ConnectorStringRequestCallback(result_, response);
    else
      ConnectorFileRequestCallback(path, result_, response);
  }
}

void FakeDeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(dm_token_, request->device_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), base::FilePath(),
                     std::move(request)),
      response_delay);
}

void FakeDeepScanningDialogDelegate::UploadFileForDeepScanning(
    BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK(!path.empty());
  DCHECK_EQ(dm_token_, request->device_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), path, std::move(request)),
      response_delay);
}

bool FakeDeepScanningDialogDelegate::use_legacy_protos() const {
  return use_legacy_protos_;
}

}  // namespace safe_browsing
