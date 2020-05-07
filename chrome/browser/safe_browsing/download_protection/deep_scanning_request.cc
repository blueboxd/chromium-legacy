// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/safe_browsing/deep_scanning_failure_modal_dialog.h"
#include "components/download/public/common/download_item.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

void DeepScanningClientResponseToDownloadCheckResult(
    const DeepScanningClientResponse& response,
    DownloadCheckResult* download_result) {
  if (response.has_malware_scan_verdict()) {
    if (response.malware_scan_verdict().verdict() ==
        MalwareDeepScanningVerdict::MALWARE) {
      *download_result = DownloadCheckResult::DANGEROUS;
      return;
    }

    if (response.malware_scan_verdict().verdict() ==
        MalwareDeepScanningVerdict::UWS) {
      *download_result = DownloadCheckResult::POTENTIALLY_UNWANTED;
      return;
    }
  }

  if (response.has_dlp_scan_verdict() &&
      response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    bool should_dlp_block = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::BLOCK;
        });
    if (should_dlp_block) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_BLOCK;
      return;
    }

    bool should_dlp_warn = std::any_of(
        response.dlp_scan_verdict().triggered_rules().begin(),
        response.dlp_scan_verdict().triggered_rules().end(),
        [](const DlpDeepScanningVerdict::TriggeredRule& rule) {
          return rule.action() == DlpDeepScanningVerdict::TriggeredRule::WARN;
        });
    if (should_dlp_warn) {
      *download_result = DownloadCheckResult::SENSITIVE_CONTENT_WARNING;
      return;
    }
  }

  if (response.has_malware_scan_verdict() &&
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::SCAN_FAILURE) {
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  if (response.has_dlp_scan_verdict() &&
      response.dlp_scan_verdict().status() != DlpDeepScanningVerdict::SUCCESS) {
    *download_result = DownloadCheckResult::UNKNOWN;
    return;
  }

  *download_result = DownloadCheckResult::DEEP_SCANNED_SAFE;
}

bool ShouldUploadForDlpScanByPolicy(download::DownloadItem* item) {
  if (!base::FeatureList::IsEnabled(kContentComplianceEnabled))
    return false;

  int check_content_compliance = g_browser_process->local_state()->GetInteger(
      prefs::kCheckContentCompliance);
  if (check_content_compliance !=
          CheckContentComplianceValues::CHECK_DOWNLOADS &&
      check_content_compliance !=
          CheckContentComplianceValues::CHECK_UPLOADS_AND_DOWNLOADS)
    return false;

  return enterprise_connectors::ConnectorsManager::GetInstance()
      ->MatchURLAgainstLegacyDlpPolicies(item->GetURL(), /*upload*/ false);
}

bool ShouldUploadForMalwareScanByPolicy(download::DownloadItem* item) {
  if (!base::FeatureList::IsEnabled(kMalwareScanEnabled))
    return false;

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item);
  if (!browser_context)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  int send_files_for_malware_check = profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck);
  if (send_files_for_malware_check !=
          SendFilesForMalwareCheckValues::SEND_DOWNLOADS &&
      send_files_for_malware_check !=
          SendFilesForMalwareCheckValues::SEND_UPLOADS_AND_DOWNLOADS)
    return false;

  return enterprise_connectors::ConnectorsManager::GetInstance()
      ->MatchURLAgainstLegacyMalwarePolicies(item->GetURL(), /*upload*/ false);
}

}  // namespace

/* static */
bool DeepScanningRequest::ShouldUploadItemByPolicy(
    download::DownloadItem* item) {
  return ShouldUploadForDlpScanByPolicy(item) ||
         ShouldUploadForMalwareScanByPolicy(item);
}

/* static */
std::vector<DeepScanningRequest::DeepScanType> DeepScanningRequest::AllScans() {
  return {DeepScanType::SCAN_DLP, DeepScanType::SCAN_MALWARE};
}

DeepScanningRequest::DeepScanningRequest(
    download::DownloadItem* item,
    DeepScanTrigger trigger,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service)
    : DeepScanningRequest(item,
                          trigger,
                          callback,
                          download_service,
                          DeepScanningRequest::AllScans()) {}

DeepScanningRequest::DeepScanningRequest(
    download::DownloadItem* item,
    DeepScanTrigger trigger,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* download_service,
    std::vector<DeepScanType> allowed_scans)
    : item_(item),
      trigger_(trigger),
      callback_(callback),
      download_service_(download_service),
      allowed_scans_(allowed_scans),
      weak_ptr_factory_(this) {
  item_->AddObserver(this);
}

DeepScanningRequest::~DeepScanningRequest() {
  item_->RemoveObserver(this);
}

void DeepScanningRequest::Start() {
  // Indicate we're now scanning the file.
  callback_.Run(DownloadCheckResult::ASYNC_SCANNING);

  auto request = std::make_unique<DownloadItemRequest>(
      item_, /*read_immediately=*/true,
      base::BindOnce(&DeepScanningRequest::OnScanComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  request->set_filename(item_->GetTargetFilePath().BaseName().AsUTF8Unsafe());

  std::string raw_digest_sha256 = item_->GetHash();
  request->set_digest(
      base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()));

  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));

  if (trigger_ == DeepScanTrigger::TRIGGER_APP_PROMPT) {
    MalwareDeepScanningClientRequest malware_request;
    malware_request.set_population(
        MalwareDeepScanningClientRequest::POPULATION_TITANIUM);
    request->set_request_malware_scan(std::move(malware_request));
  } else if (trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    policy::DMToken dm_token = GetDMToken(profile);
    request->set_dm_token(dm_token.value());

    if (ShouldUploadForDlpScanByPolicy(item_) &&
        ScanIsAllowed(DeepScanType::SCAN_DLP)) {
      DlpDeepScanningClientRequest dlp_request;
      dlp_request.set_content_source(
          DlpDeepScanningClientRequest::FILE_DOWNLOAD);
      if (item_->GetTabUrl().is_valid()) {
        dlp_request.set_url(item_->GetTabUrl().spec());
      }
      request->set_request_dlp_scan(std::move(dlp_request));
    }

    if (ShouldUploadForMalwareScanByPolicy(item_) &&
        ScanIsAllowed(DeepScanType::SCAN_MALWARE)) {
      MalwareDeepScanningClientRequest malware_request;
      malware_request.set_population(
          MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
      request->set_request_malware_scan(std::move(malware_request));
    }
  }

  upload_start_time_ = base::TimeTicks::Now();
  BinaryUploadService* binary_upload_service =
      download_service_->GetBinaryUploadService(profile);
  if (binary_upload_service) {
    binary_upload_service->MaybeUploadForDeepScanning(std::move(request));
  } else {
    OnScanComplete(BinaryUploadService::Result::UNKNOWN,
                   DeepScanningClientResponse());
  }
}

void DeepScanningRequest::OnScanComplete(BinaryUploadService::Result result,
                                         DeepScanningClientResponse response) {
  RecordDeepScanMetrics(
      /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
      /*duration=*/base::TimeTicks::Now() - upload_start_time_,
      /*total_size=*/item_->GetTotalBytes(), /*result=*/result,
      /*response=*/response);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (profile && trigger_ == DeepScanTrigger::TRIGGER_POLICY) {
    std::string raw_digest_sha256 = item_->GetHash();
    MaybeReportDeepScanningVerdict(
        profile, item_->GetURL(), item_->GetTargetFilePath().AsUTF8Unsafe(),
        base::HexEncode(raw_digest_sha256.data(), raw_digest_sha256.size()),
        item_->GetMimeType(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        item_->GetTotalBytes(), result, response);
  }

  DownloadCheckResult download_result = DownloadCheckResult::UNKNOWN;
  if (result == BinaryUploadService::Result::SUCCESS) {
    DeepScanningClientResponseToDownloadCheckResult(response, &download_result);
  } else if (trigger_ == DeepScanTrigger::TRIGGER_APP_PROMPT &&
             MaybeShowDeepScanFailureModalDialog(
                 base::BindOnce(&DeepScanningRequest::Start,
                                weak_ptr_factory_.GetWeakPtr()),
                 base::BindOnce(&DeepScanningRequest::FinishRequest,
                                weak_ptr_factory_.GetWeakPtr(),
                                DownloadCheckResult::UNKNOWN),
                 base::BindOnce(&DeepScanningRequest::OpenDownload,
                                weak_ptr_factory_.GetWeakPtr()))) {
    return;
  } else if (result == BinaryUploadService::Result::FILE_TOO_LARGE ||
             result == BinaryUploadService::Result::FILE_ENCRYPTED ||
             result == BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE) {
    auto settings =
        enterprise_connectors::ConnectorsManager::GetInstance()
            ->GetAnalysisSettings(
                item_->GetURL(),
                enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED)
            .value_or(enterprise_connectors::AnalysisSettings());
    if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
      if (settings.block_large_files)
        download_result = DownloadCheckResult::BLOCKED_TOO_LARGE;
    } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED) {
      if (settings.block_password_protected_files)
        download_result = DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED;
    } else if (result == BinaryUploadService::Result::UNSUPPORTED_FILE_TYPE) {
      if (settings.block_unsupported_file_types)
        download_result = DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE;
    }
  }

  FinishRequest(download_result);
}

void DeepScanningRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

void DeepScanningRequest::FinishRequest(DownloadCheckResult result) {
  callback_.Run(result);
  weak_ptr_factory_.InvalidateWeakPtrs();
  item_->RemoveObserver(this);
  download_service_->RequestFinished(this);
}

bool DeepScanningRequest::MaybeShowDeepScanFailureModalDialog(
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback,
    base::OnceClosure open_now_callback) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  if (!profile)
    return false;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  if (!browser)
    return false;

  DeepScanningFailureModalDialog::ShowForWebContents(
      browser->tab_strip_model()->GetActiveWebContents(),
      std::move(accept_callback), std::move(cancel_callback),
      std::move(open_now_callback));
  return true;
}

void DeepScanningRequest::OpenDownload() {
  item_->OpenDownload();
  FinishRequest(DownloadCheckResult::UNKNOWN);
}

bool DeepScanningRequest::ScanIsAllowed(DeepScanType scan) {
  return base::Contains(allowed_scans_, scan);
}

}  // namespace safe_browsing
