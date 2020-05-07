// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {
namespace {

const int kScanningTimeoutSeconds = 5 * 60;  // 5 minutes

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbAppUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/app";

bool IsAdvancedProtectionRequest(const BinaryUploadService::Request& request) {
  return !request.deep_scanning_request().has_dlp_scan_request() &&
         request.deep_scanning_request().has_malware_scan_request() &&
         request.deep_scanning_request().malware_scan_request().population() ==
             MalwareDeepScanningClientRequest::POPULATION_TITANIUM;
}

std::string ResultToString(BinaryUploadService::Result result) {
  switch (result) {
    case BinaryUploadService::Result::UNKNOWN:
      return "UNKNOWN";
    case BinaryUploadService::Result::SUCCESS:
      return "SUCCESS";
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      return "UPLOAD_FAILURE";
    case BinaryUploadService::Result::TIMEOUT:
      return "TIMEOUT";
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "FAILED_TO_GET_TOKEN";
    case BinaryUploadService::Result::UNAUTHORIZED:
      return "UNAUTHORIZED";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FILE_ENCRYPTED";
    case BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE:
      return "UNSUPPORTED_FILE_TYPE";
  }
}

}  // namespace

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(BinaryFCMService::Create(profile)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    std::unique_ptr<BinaryFCMService> binary_fcm_service)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(std::move(binary_fcm_service)),
      profile_(profile),
      weakptr_factory_(this) {}

BinaryUploadService::~BinaryUploadService() {}

void BinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsAdvancedProtectionRequest(*request)) {
    MaybeUploadForDeepScanningCallback(
        std::move(request),
        /*authorized=*/safe_browsing::AdvancedProtectionStatusManagerFactory::
            GetForProfile(profile_)
                ->IsUnderAdvancedProtection());
    return;
  }

  if (!can_upload_enterprise_data_.has_value()) {
    IsAuthorized(
        base::BindOnce(&BinaryUploadService::MaybeUploadForDeepScanningCallback,
                       weakptr_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  MaybeUploadForDeepScanningCallback(std::move(request),
                                     can_upload_enterprise_data_.value());
}

void BinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadService::Request> request,
    bool authorized) {
  // Ignore the request if the browser cannot upload data.
  if (!authorized) {
    // TODO(crbug/1028133): Add extra logic to handle UX for non-authorized
    // users.
    request->FinishRequest(Result::UNAUTHORIZED, DeepScanningClientResponse());
    return;
  }
  UploadForDeepScanning(std::move(request));
}

void BinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Request* raw_request = request.get();
  active_requests_[raw_request] = std::move(request);
  start_times_[raw_request] = base::TimeTicks::Now();

  std::string token = base::RandBytesAsString(128);
  token = base::HexEncode(token.data(), token.size());
  active_tokens_[raw_request] = token;
  raw_request->set_request_token(token);

  if (!binary_fcm_service_) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&BinaryUploadService::FinishRequest,
                                  weakptr_factory_.GetWeakPtr(), raw_request,
                                  Result::FAILED_TO_GET_TOKEN,
                                  DeepScanningClientResponse()));
    return;
  }

  binary_fcm_service_->SetCallbackForToken(
      token, base::BindRepeating(&BinaryUploadService::OnGetResponse,
                                 weakptr_factory_.GetWeakPtr(), raw_request));
  binary_fcm_service_->GetInstanceID(
      base::BindOnce(&BinaryUploadService::OnGetInstanceID,
                     weakptr_factory_.GetWeakPtr(), raw_request));
  active_timers_[raw_request] = std::make_unique<base::OneShotTimer>();
  active_timers_[raw_request]->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kScanningTimeoutSeconds),
      base::BindOnce(&BinaryUploadService::OnTimeout,
                     weakptr_factory_.GetWeakPtr(), raw_request));
}

void BinaryUploadService::OnGetInstanceID(Request* request,
                                          const std::string& instance_id) {
  if (!IsActive(request))
    return;

  if (instance_id == BinaryFCMService::kInvalidId) {
    FinishRequest(request, Result::FAILED_TO_GET_TOKEN,
                  DeepScanningClientResponse());
    return;
  }

  request->set_fcm_token(instance_id);
  request->GetRequestData(base::BindOnce(&BinaryUploadService::OnGetRequestData,
                                         weakptr_factory_.GetWeakPtr(),
                                         request));
}

void BinaryUploadService::OnGetRequestData(Request* request,
                                           Result result,
                                           const Request::Data& data) {
  if (!IsActive(request))
    return;

  if (result != Result::SUCCESS) {
    FinishRequest(request, result, DeepScanningClientResponse());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_binary_upload", R"(
        semantics {
          sender: "Safe Browsing Download Protection"
          description:
            "For users with the enterprise policy "
            "SendFilesForMalwareCheck set, when a file is "
            "downloaded, Chrome will upload that file to Safe Browsing for "
            "detailed scanning."
          trigger:
            "The browser will upload the file to Google when "
            "the user downloads a file, and the enterprise policy "
            "SendFilesForMalwareCheck is set."
          data:
            "The downloaded file."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 0
            }
          }
          chrome_policy {
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 1
            }
          }
        }
        comments: "Setting SendFilesForMalwareCheck to 0 (Do not scan "
          "downloads) or 1 (Forbid the scanning of downloads) will disable "
          "this feature"
        )");

  std::string metadata;
  request->deep_scanning_request().SerializeToString(&metadata);
  base::Base64Encode(metadata, &metadata);

  auto upload_request = MultipartUploadRequest::Create(
      url_loader_factory_, GetUploadUrl(IsAdvancedProtectionRequest(*request)),
      metadata, data.contents, traffic_annotation,
      base::BindOnce(&BinaryUploadService::OnUploadComplete,
                     weakptr_factory_.GetWeakPtr(), request));
  upload_request->Start();
  active_uploads_[request] = std::move(upload_request);

  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->deep_scanning_request());
}

void BinaryUploadService::OnUploadComplete(Request* request,
                                           bool success,
                                           const std::string& response_data) {
  if (!IsActive(request))
    return;

  if (!success) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  DeepScanningClientResponse());
    return;
  }

  DeepScanningClientResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  DeepScanningClientResponse());
    return;
  }

  active_uploads_.erase(request);

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request, response);
}

void BinaryUploadService::OnGetResponse(Request* request,
                                        DeepScanningClientResponse response) {
  if (!IsActive(request))
    return;

  if (response.has_dlp_scan_verdict()) {
    VLOG(1) << "Request " << request->deep_scanning_request().request_token()
            << " finished DLP scanning";
    received_dlp_verdicts_[request].reset(response.release_dlp_scan_verdict());
  }

  if (response.has_malware_scan_verdict()) {
    VLOG(1) << "Request " << request->deep_scanning_request().request_token()
            << " finished malware scanning";
    received_malware_verdicts_[request].reset(
        response.release_malware_scan_verdict());
  }

  MaybeFinishRequest(request);
}

void BinaryUploadService::MaybeFinishRequest(Request* request) {
  bool requested_dlp_scan_response =
      request->deep_scanning_request().has_dlp_scan_request();
  auto received_dlp_response = received_dlp_verdicts_.find(request);
  if (requested_dlp_scan_response &&
      received_dlp_response == received_dlp_verdicts_.end()) {
    VLOG(1) << "Request " << request->deep_scanning_request().request_token()
            << " is waiting for DLP scanning to complete.";
    return;
  }

  bool requested_malware_scan_response =
      request->deep_scanning_request().has_malware_scan_request();
  auto received_malware_response = received_malware_verdicts_.find(request);
  if (requested_malware_scan_response &&
      received_malware_response == received_malware_verdicts_.end()) {
    VLOG(1) << "Request " << request->deep_scanning_request().request_token()
            << " is waiting for malware scanning to complete.";
    return;
  }

  DeepScanningClientResponse response;
  response.set_token(request->deep_scanning_request().request_token());
  if (requested_dlp_scan_response) {
    // Transfers ownership of the DLP response to |response|.
    response.set_allocated_dlp_scan_verdict(
        received_dlp_response->second.release());
  }

  if (requested_malware_scan_response) {
    // Transfers ownership of the malware response to |response|.
    response.set_allocated_malware_scan_verdict(
        received_malware_response->second.release());
  }

  FinishRequest(request, Result::SUCCESS, std::move(response));
}

void BinaryUploadService::OnTimeout(Request* request) {
  if (IsActive(request))
    FinishRequest(request, Result::TIMEOUT, DeepScanningClientResponse());
}

void BinaryUploadService::FinishRequest(Request* request,
                                        Result result,
                                        DeepScanningClientResponse response) {
  RecordRequestMetrics(request, result, response);

  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->deep_scanning_request());
  WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request], ResultToString(result), response);

  std::string instance_id =
      request->deep_scanning_request().fcm_notification_token();

  request->FinishRequest(result, response);
  active_requests_.erase(request);
  active_timers_.erase(request);
  active_uploads_.erase(request);
  received_malware_verdicts_.erase(request);
  received_dlp_verdicts_.erase(request);

  auto token_it = active_tokens_.find(request);
  DCHECK(token_it != active_tokens_.end());
  if (binary_fcm_service_) {
    binary_fcm_service_->ClearCallbackForToken(token_it->second);

    // The BinaryFCMService will handle all recoverable errors. In case of
    // unrecoverable error, there's nothing we can do here.
    binary_fcm_service_->UnregisterInstanceID(instance_id, base::DoNothing());
  }

  active_tokens_.erase(token_it);
}

void BinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const DeepScanningClientResponse& response) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                base::TimeTicks::Now() - start_times_[request],
                                base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(6), 50);

  if (response.has_malware_scan_verdict()) {
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.MalwareResult",
                              response.malware_scan_verdict().verdict() !=
                                  MalwareDeepScanningVerdict::SCAN_FAILURE);
  }

  if (response.has_dlp_scan_verdict()) {
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.DlpResult",
                              response.dlp_scan_verdict().status() ==
                                  DlpDeepScanningVerdict::SUCCESS);
  }
}

BinaryUploadService::Request::Data::Data() = default;

BinaryUploadService::Request::Request(Callback callback)
    : callback_(std::move(callback)) {}

BinaryUploadService::Request::~Request() {}

void BinaryUploadService::Request::set_request_dlp_scan(
    DlpDeepScanningClientRequest dlp_request) {
  *deep_scanning_request_.mutable_dlp_scan_request() = std::move(dlp_request);
}

void BinaryUploadService::Request::set_request_malware_scan(
    MalwareDeepScanningClientRequest malware_request) {
  *deep_scanning_request_.mutable_malware_scan_request() =
      std::move(malware_request);
}

void BinaryUploadService::Request::set_fcm_token(const std::string& token) {
  deep_scanning_request_.set_fcm_notification_token(token);
}

void BinaryUploadService::Request::set_dm_token(const std::string& token) {
  deep_scanning_request_.set_dm_token(token);
}

void BinaryUploadService::Request::set_request_token(const std::string& token) {
  deep_scanning_request_.set_request_token(token);
}

void BinaryUploadService::Request::set_filename(const std::string& filename) {
  deep_scanning_request_.set_filename(filename);
}

void BinaryUploadService::Request::set_digest(const std::string& digest) {
  deep_scanning_request_.set_digest(digest);
}

void BinaryUploadService::Request::FinishRequest(
    Result result,
    DeepScanningClientResponse response) {
  std::move(callback_).Run(result, response);
}

bool BinaryUploadService::IsActive(Request* request) {
  return (active_requests_.find(request) != active_requests_.end());
}

class ValidateDataUploadRequest : public BinaryUploadService::Request {
 public:
  explicit ValidateDataUploadRequest(BinaryUploadService::Callback callback)
      : BinaryUploadService::Request(std::move(callback)) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                          BinaryUploadService::Request::Data());
}

void BinaryUploadService::IsAuthorized(AuthorizationCallback callback) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromHours(24), this,
                 &BinaryUploadService::ResetAuthorizationData);
  }

  if (!can_upload_enterprise_data_.has_value()) {
    // Send a request to check if the browser can upload data.
    authorization_callbacks_.push_back(std::move(callback));
    if (!pending_validate_data_upload_request_) {
      auto dm_token = GetDMToken(profile_);
      if (!dm_token.is_valid()) {
        can_upload_enterprise_data_ = false;
        RunAuthorizationCallbacks();
        return;
      }

      pending_validate_data_upload_request_ = true;
      auto request = std::make_unique<ValidateDataUploadRequest>(base::BindOnce(
          &BinaryUploadService::ValidateDataUploadRequestCallback,
          weakptr_factory_.GetWeakPtr()));
      request->set_dm_token(dm_token.value());
      UploadForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_.value());
}

void BinaryUploadService::ValidateDataUploadRequestCallback(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  pending_validate_data_upload_request_ = false;
  can_upload_enterprise_data_ = result == BinaryUploadService::Result::SUCCESS;
  RunAuthorizationCallbacks();
}

void BinaryUploadService::RunAuthorizationCallbacks() {
  DCHECK(can_upload_enterprise_data_.has_value());
  for (auto& callback : authorization_callbacks_) {
    std::move(callback).Run(can_upload_enterprise_data_.value());
  }
  authorization_callbacks_.clear();
}

void BinaryUploadService::ResetAuthorizationData() {
  // Setting |can_upload_enterprise_data_| to base::nullopt will make the next
  // call to IsAuthorized send out a request to validate data uploads.
  can_upload_enterprise_data_ = base::nullopt;

  // Call IsAuthorized  to update |can_upload_enterprise_data_| right away.
  IsAuthorized(base::DoNothing());
}

void BinaryUploadService::Shutdown() {
  if (binary_fcm_service_)
    binary_fcm_service_->Shutdown();
}

void BinaryUploadService::SetAuthForTesting(bool authorized) {
  can_upload_enterprise_data_ = authorized;
}

// static
GURL BinaryUploadService::GetUploadUrl(bool is_advanced_protection) {
  return is_advanced_protection ? GURL(kSbAppUploadUrl)
                                : GURL(kSbEnterpriseUploadUrl);
}

}  // namespace safe_browsing
