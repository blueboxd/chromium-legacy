// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_source_request.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/web_contents.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

namespace safe_browsing {

namespace {

// Global pointer of factory function (RepeatingCallback) used to create
// instances of DeepScanningDialogDelegate in tests.  !is_null() only in tests.
DeepScanningDialogDelegate::Factory* GetFactoryStorage() {
  static base::NoDestructor<DeepScanningDialogDelegate::Factory> factory;
  return factory.get();
}

// A BinaryUploadService::Request implementation that gets the data to scan
// from a string.
class StringSourceRequest : public BinaryUploadService::Request {
 public:
  StringSourceRequest(std::string text, BinaryUploadService::Callback callback);
  ~StringSourceRequest() override;

  StringSourceRequest(const StringSourceRequest&) = delete;
  StringSourceRequest& operator=(const StringSourceRequest&) = delete;

  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  BinaryUploadService::Result result_ =
      BinaryUploadService::Result::FILE_TOO_LARGE;
};

StringSourceRequest::StringSourceRequest(std::string text,
                                         BinaryUploadService::Callback callback)
    : Request(std::move(callback)) {
  // Only remember strings less than the maximum allowed.
  if (text.size() < BinaryUploadService::kMaxUploadSizeBytes) {
    data_.contents = std::move(text);
    result_ = BinaryUploadService::Result::SUCCESS;
  }
}

StringSourceRequest::~StringSourceRequest() = default;

void StringSourceRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

bool DlpTriggeredRulesOK(
    const ::safe_browsing::DlpDeepScanningVerdict& verdict) {
  // No status returns true since this function is called even when the server
  // doesn't return a DLP scan verdict.
  if (!verdict.has_status())
    return true;

  if (verdict.status() != DlpDeepScanningVerdict::SUCCESS)
    return false;

  for (int i = 0; i < verdict.triggered_rules_size(); ++i) {
    if (verdict.triggered_rules(i).action() ==
            DlpDeepScanningVerdict::TriggeredRule::BLOCK ||
        verdict.triggered_rules(i).action() ==
            DlpDeepScanningVerdict::TriggeredRule::WARN) {
      return false;
    }
  }
  return true;
}

bool ShouldShowWarning(const DlpDeepScanningVerdict& verdict) {
  // Show a warning if one of the triggered rules is WARN and no other rule is
  // BLOCK.
  auto rules = verdict.triggered_rules();
  bool no_block = std::all_of(rules.begin(), rules.end(), [](const auto& rule) {
    return rule.action() != DlpDeepScanningVerdict::TriggeredRule::BLOCK;
  });
  bool warning = std::any_of(rules.begin(), rules.end(), [](const auto& rule) {
    return rule.action() == DlpDeepScanningVerdict::TriggeredRule::WARN;
  });
  return no_block && warning;
}

std::string GetFileMimeType(base::FilePath path) {
  // TODO(crbug.com/1013252): Obtain a more accurate MimeType by parsing the
  // file content.
  base::FilePath::StringType ext = path.FinalExtension();
  if (ext.empty())
    return "";

  if (ext[0] == FILE_PATH_LITERAL('.'))
    ext = ext.substr(1);

  std::string mime_type;
  net::GetMimeTypeFromExtension(ext, &mime_type);
  return mime_type;
}

bool* UIEnabledStorage() {
  static bool enabled = true;
  return &enabled;
}

}  // namespace

DeepScanningDialogDelegate::Data::Data() = default;
DeepScanningDialogDelegate::Data::Data(Data&& other) = default;
DeepScanningDialogDelegate::Data::~Data() = default;

DeepScanningDialogDelegate::Result::Result() = default;
DeepScanningDialogDelegate::Result::Result(Result&& other) = default;
DeepScanningDialogDelegate::Result::~Result() = default;

DeepScanningDialogDelegate::FileInfo::FileInfo() = default;
DeepScanningDialogDelegate::FileInfo::FileInfo(FileInfo&& other) = default;
DeepScanningDialogDelegate::FileInfo::~FileInfo() = default;

DeepScanningDialogDelegate::FileContents::FileContents() = default;
DeepScanningDialogDelegate::FileContents::FileContents(
    BinaryUploadService::Result result)
    : result(result) {}

DeepScanningDialogDelegate::FileContents::FileContents(FileContents&& other) =
    default;
DeepScanningDialogDelegate::FileContents&
DeepScanningDialogDelegate::FileContents::operator=(
    DeepScanningDialogDelegate::FileContents&& other) = default;
DeepScanningDialogDelegate::~DeepScanningDialogDelegate() = default;

void DeepScanningDialogDelegate::BypassWarnings() {
  if (callback_.is_null())
    return;

  // Mark the full text as complying and report a warning bypass.
  if (text_warning_) {
    std::fill(result_.text_results.begin(), result_.text_results.end(), true);

    int64_t content_size = 0;
    for (const base::string16& entry : data_.text)
      content_size += (entry.size() * sizeof(base::char16));

    ReportSensitiveDataWarningBypass(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        web_contents_->GetLastCommittedURL(), "Text data", std::string(),
        "text/plain",
        extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
        content_size, text_response_.dlp_scan_verdict());
  }

  // Mark every "warning" file as complying and report a warning bypass.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;
    result_.paths_results[index] = true;

    ReportSensitiveDataWarningBypass(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
        web_contents_->GetLastCommittedURL(), data_.paths[index].AsUTF8Unsafe(),
        file_info_[index].sha256, file_info_[index].mime_type,
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
        file_info_[index].size, warning.second.dlp_scan_verdict());
  }

  RunCallback();
}

void DeepScanningDialogDelegate::Cancel(bool warning) {
  if (callback_.is_null())
    return;

  // Don't report this upload as cancelled if the user didn't bypass the
  // warning.
  if (!warning) {
    RecordDeepScanMetrics(access_point_,
                          base::TimeTicks::Now() - upload_start_time_, 0,
                          "CancelledByUser", false);
  }

  // Make sure to reject everything.
  FillAllResultsWith(false);
  RunCallback();
}

// static
bool DeepScanningDialogDelegate::ResultShouldAllowDataUse(
    BinaryUploadService::Result result,
    const enterprise_connectors::AnalysisSettings& settings) {
  // Keep this implemented as a switch instead of a simpler if statement so that
  // new values added to BinaryUploadService::Result cause a compiler error.
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
    // UNAUTHORIZED allows data usage since it's a result only obtained if the
    // browser is not authorized to perform deep scanning. It does not make
    // sense to block data in this situation since no actual scanning of the
    // data was performed, so it's allowed.
    case BinaryUploadService::Result::UNAUTHORIZED:
    case BinaryUploadService::Result::UNKNOWN:
      return true;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return !settings.block_large_files;

    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return !settings.block_password_protected_files;

    case BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE:
      return !settings.block_unsupported_file_types;
  }
}

// static
bool DeepScanningDialogDelegate::IsEnabled(
    Profile* profile,
    GURL url,
    Data* data,
    enterprise_connectors::AnalysisConnector connector) {
  // If this is an incognitio profile, don't perform scans.
  if (profile->IsOffTheRecord())
    return false;

  // If there's no valid DM token, the upload will fail.
  if (!GetDMToken(profile).is_valid())
    return false;

  data->do_dlp_scan = base::FeatureList::IsEnabled(kContentComplianceEnabled);
  data->do_malware_scan = base::FeatureList::IsEnabled(kMalwareScanEnabled);

  // If neither DLP or malware scanning is enabled by features, don't perform
  // scans.
  if (!data->do_dlp_scan && !data->do_malware_scan)
    return false;

  // If the settings arent't obtained by the corresponding connector, check
  // the legacy DLP and Malware policies.
  if (!enterprise_connectors::ConnectorsManager::GetInstance()
           ->IsConnectorEnabled(connector)) {
    int state = g_browser_process->local_state()->GetInteger(
        prefs::kCheckContentCompliance);
    data->do_dlp_scan &=
        (state == CHECK_UPLOADS || state == CHECK_UPLOADS_AND_DOWNLOADS);

    state = profile->GetPrefs()->GetInteger(
        prefs::kSafeBrowsingSendFilesForMalwareCheck);
    data->do_malware_scan &=
        (state == SEND_UPLOADS || state == SEND_UPLOADS_AND_DOWNLOADS);

    if (!data->do_dlp_scan && !data->do_malware_scan)
      return false;
  }

  // Check that |url| matches the appropriate URL patterns by getting settings.
  // No settings means no matches were found.
  auto settings = enterprise_connectors::ConnectorsManager::GetInstance()
                      ->GetAnalysisSettings(url, connector);
  if (!settings.has_value()) {
    data->do_dlp_scan = false;
    data->do_malware_scan = false;
    return false;
  }

  data->settings = std::move(settings.value());
  data->do_dlp_scan &= (data->settings.tags.count("dlp") == 1);
  data->do_malware_scan &= (data->settings.tags.count("malware") == 1);

  if (url.is_valid())
    data->url = url;

  return data->do_dlp_scan || data->do_malware_scan;
}

// static
void DeepScanningDialogDelegate::ShowForWebContents(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    DeepScanAccessPoint access_point) {
  Factory* testing_factory = GetFactoryStorage();
  bool wait_for_verdict = data.settings.block_until_verdict ==
                          enterprise_connectors::BlockUntilVerdict::BLOCK;

  // Using new instead of std::make_unique<> to access non public constructor.
  auto delegate = testing_factory->is_null()
                      ? std::unique_ptr<DeepScanningDialogDelegate>(
                            new DeepScanningDialogDelegate(
                                web_contents, std::move(data),
                                std::move(callback), access_point))
                      : testing_factory->Run(web_contents, std::move(data),
                                             std::move(callback));

  bool work_being_done = delegate->UploadData();

  // Only show UI if work is being done in the background, the user must
  // wait for a verdict.
  bool show_ui = work_being_done && wait_for_verdict && (*UIEnabledStorage());

  // If the UI is enabled, create the modal dialog.
  if (show_ui) {
    DeepScanningDialogDelegate* delegate_ptr = delegate.get();
    bool is_file_scan = !delegate_ptr->data_.paths.empty();
    delegate_ptr->dialog_ =
        new DeepScanningDialogViews(std::move(delegate), web_contents,
                                    std::move(access_point), is_file_scan);
    return;
  }

  if (!wait_for_verdict || !work_being_done) {
    // The UI will not be shown but the policy is set to not wait for the
    // verdict, or no scans need to be performed.  Inform the caller that they
    // may proceed.
    //
    // Supporting "wait for verdict" while not showing a UI makes writing tests
    // for callers of this code easier.
    delegate->FillAllResultsWith(true);
    delegate->RunCallback();
  }

  // Upload service callback will delete the delegate.
  if (work_being_done)
    delegate.release();
}

// static
void DeepScanningDialogDelegate::SetFactoryForTesting(Factory factory) {
  *GetFactoryStorage() = factory;
}

// static
void DeepScanningDialogDelegate::ResetFactoryForTesting() {
  if (GetFactoryStorage())
    GetFactoryStorage()->Reset();
}

// static
void DeepScanningDialogDelegate::DisableUIForTesting() {
  *UIEnabledStorage() = false;
}

DeepScanningDialogDelegate::DeepScanningDialogDelegate(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    DeepScanAccessPoint access_point)
    : web_contents_(web_contents),
      data_(std::move(data)),
      callback_(std::move(callback)),
      access_point_(access_point) {
  DCHECK(web_contents_);
  result_.text_results.resize(data_.text.size(), false);
  result_.paths_results.resize(data_.paths.size(), false);
  file_info_.resize(data_.paths.size());
}

void DeepScanningDialogDelegate::StringRequestCallback(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  int64_t content_size = 0;
  for (const base::string16& entry : data_.text)
    content_size += (entry.size() * sizeof(base::char16));
  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        content_size, result, response);

  MaybeReportDeepScanningVerdict(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      web_contents_->GetLastCommittedURL(), "Text data", std::string(),
      "text/plain",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      content_size, result, response);

  text_request_complete_ = true;
  bool text_complies = ResultShouldAllowDataUse(result, data_.settings) &&
                       DlpTriggeredRulesOK(response.dlp_scan_verdict());
  std::fill(result_.text_results.begin(), result_.text_results.end(),
            text_complies);

  if (!text_complies) {
    if (ShouldShowWarning(response.dlp_scan_verdict())) {
      text_warning_ = true;
      text_response_ = std::move(response);
      UpdateFinalResult(DeepScanningFinalResult::WARNING);
    } else {
      UpdateFinalResult(DeepScanningFinalResult::FAILURE);
    }
  }

  MaybeCompleteScanRequest();
}

void DeepScanningDialogDelegate::CompleteFileRequestCallback(
    size_t index,
    base::FilePath path,
    BinaryUploadService::Result result,
    DeepScanningClientResponse response,
    std::string mime_type) {
  file_info_[index].mime_type = mime_type;
  MaybeReportDeepScanningVerdict(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      web_contents_->GetLastCommittedURL(), path.AsUTF8Unsafe(),
      file_info_[index].sha256, mime_type,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      file_info_[index].size, result, response);

  bool dlp_ok = DlpTriggeredRulesOK(response.dlp_scan_verdict());
  bool malware_ok = true;
  if (response.has_malware_scan_verdict()) {
    malware_ok = response.malware_scan_verdict().verdict() ==
                 MalwareDeepScanningVerdict::CLEAN;
  }

  bool file_complies =
      ResultShouldAllowDataUse(result, data_.settings) && dlp_ok && malware_ok;
  result_.paths_results[index] = file_complies;

  ++file_result_count_;

  if (!file_complies) {
    if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
      UpdateFinalResult(DeepScanningFinalResult::LARGE_FILES);
    } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED) {
      UpdateFinalResult(DeepScanningFinalResult::ENCRYPTED_FILES);
    } else if (ShouldShowWarning(response.dlp_scan_verdict())) {
      file_warnings_[index] = std::move(response);
      UpdateFinalResult(DeepScanningFinalResult::WARNING);
    } else {
      UpdateFinalResult(DeepScanningFinalResult::FAILURE);
    }
  }

  MaybeCompleteScanRequest();
}

void DeepScanningDialogDelegate::FileRequestCallback(
    base::FilePath path,
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  // Find the path in the set of files that are being scanned.
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);

  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        file_info_[index].size, result, response);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileMimeType, path),
      base::BindOnce(&DeepScanningDialogDelegate::CompleteFileRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), index, path, result,
                     response));
}

bool DeepScanningDialogDelegate::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();
  if (data_.do_dlp_scan) {
    // Create a string data source based on all the text.
    std::string full_text;
    for (const auto& text : data_.text)
      full_text.append(base::UTF16ToUTF8(text));

    text_request_complete_ = full_text.empty();
    if (!text_request_complete_) {
      auto request = std::make_unique<StringSourceRequest>(
          std::move(full_text),
          base::BindOnce(&DeepScanningDialogDelegate::StringRequestCallback,
                         weak_ptr_factory_.GetWeakPtr()));

      PrepareRequest(DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
                     request.get());
      UploadTextForDeepScanning(std::move(request));
    }
  } else {
    // Text data sent only for content compliance.
    text_request_complete_ = true;
  }

  // Create a file request for each file.
  for (const base::FilePath& path : data_.paths)
    PrepareFileRequest(path);

  return !text_request_complete_ || file_result_count_ != data_.paths.size();
}

void DeepScanningDialogDelegate::PrepareFileRequest(
    const base::FilePath& path) {
  auto request = std::make_unique<FileSourceRequest>(
      path, base::BindOnce(&DeepScanningDialogDelegate::FileRequestCallback,
                           weak_ptr_factory_.GetWeakPtr(), path));

  FileSourceRequest* request_raw = request.get();
  PrepareRequest(DlpDeepScanningClientRequest::FILE_UPLOAD, request_raw);
  request_raw->GetRequestData(
      base::BindOnce(&DeepScanningDialogDelegate::OnGotFileInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request), path));
}

void DeepScanningDialogDelegate::PrepareRequest(
    DlpDeepScanningClientRequest::ContentSource trigger,
    BinaryUploadService::Request* request) {
  if (data_.do_dlp_scan) {
    DlpDeepScanningClientRequest dlp_request;
    dlp_request.set_content_source(trigger);
    dlp_request.set_url(data_.url.spec());
    request->set_request_dlp_scan(std::move(dlp_request));
  }

  if (data_.do_malware_scan) {
    MalwareDeepScanningClientRequest malware_request;
    malware_request.set_population(
        MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
    request->set_request_malware_scan(std::move(malware_request));
  }

  request->set_dm_token(GetDMToken(Profile::FromBrowserContext(
                                       web_contents_->GetBrowserContext()))
                            .value());
}

void DeepScanningDialogDelegate::FillAllResultsWith(bool status) {
  std::fill(result_.text_results.begin(), result_.text_results.end(), status);
  std::fill(result_.paths_results.begin(), result_.paths_results.end(), status);
}

BinaryUploadService* DeepScanningDialogDelegate::GetBinaryUploadService() {
  return BinaryUploadServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

void DeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void DeepScanningDialogDelegate::UploadFileForDeepScanning(
    BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK(
      !data_.do_dlp_scan ||
      (DlpDeepScanningClientRequest::FILE_UPLOAD ==
       request->deep_scanning_request().dlp_scan_request().content_source()));

  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

bool DeepScanningDialogDelegate::UpdateDialog() {
  if (!dialog_)
    return false;

  dialog_->ShowResult(final_result_);
  return true;
}

void DeepScanningDialogDelegate::MaybeCompleteScanRequest() {
  if (!text_request_complete_ || file_result_count_ < data_.paths.size())
    return;

  // If showing the warning message, wait before running the callback. The
  // callback will be called either in BypassWarnings or Cancel.
  if (final_result_ != DeepScanningFinalResult::WARNING)
    RunCallback();

  if (!UpdateDialog()) {
    // No UI was shown.  Delete |this| to cleanup.
    delete this;
  }
}

void DeepScanningDialogDelegate::RunCallback() {
  if (!callback_.is_null())
    std::move(callback_).Run(data_, result_);
}

void DeepScanningDialogDelegate::OnGotFileInfo(
    std::unique_ptr<BinaryUploadService::Request> request,
    const base::FilePath& path,
    BinaryUploadService::Result result,
    const BinaryUploadService::Request::Data& data) {
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);
  file_info_[index].sha256 = data.hash;
  file_info_[index].size = data.size;

  // If a non-SUCCESS result was previously obtained, it means the file has some
  // property (too large, unsupported file type, encrypted, ...) that make its
  // upload pointless, so the request should finish early.
  if (result != BinaryUploadService::Result::SUCCESS) {
    request->FinishRequest(result, DeepScanningClientResponse());
    return;
  }

  UploadFileForDeepScanning(result, data_.paths[index], std::move(request));
}

void DeepScanningDialogDelegate::UpdateFinalResult(
    DeepScanningFinalResult result) {
  if (result < final_result_)
    final_result_ = result;
}

}  // namespace safe_browsing
