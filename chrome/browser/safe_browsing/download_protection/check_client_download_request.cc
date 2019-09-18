// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_item_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

using content::BrowserThread;

namespace {

// TODO(drubery): This function would be simpler if the ClientDownloadResponse
// and MalwareDeepScanningVerdict used the same enum.
std::string MalwareVerdictToThreatType(
    MalwareDeepScanningVerdict::Verdict verdict) {
  switch (verdict) {
    case MalwareDeepScanningVerdict::CLEAN:
      return "SAFE";
    case MalwareDeepScanningVerdict::UWS:
      return "POTENTIALLY_UNWANTED";
    case MalwareDeepScanningVerdict::MALWARE:
      return "DANGEROUS";
    case MalwareDeepScanningVerdict::VERDICT_UNSPECIFIED:
    default:
      return "UNKNOWN";
  }
}

}  // namespace

void MaybeReportDownloadDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnLargeUnscannedFileEvent(url, file_name, download_digest_sha256);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kUnsafeEventsReportingEnabled))
    return;

  if (response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::UWS ||
      response.malware_scan_verdict().verdict() ==
          MalwareDeepScanningVerdict::MALWARE) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
        ->OnDangerousDeepScanningResult(
            url, file_name, download_digest_sha256,
            MalwareVerdictToThreatType(
                response.malware_scan_verdict().verdict()));
  }

  if (response.dlp_scan_verdict().status() == DlpDeepScanningVerdict::SUCCESS) {
    if (!response.dlp_scan_verdict().triggered_rules().empty()) {
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile)
          ->OnSensitiveDataEvent(response.dlp_scan_verdict(), url, file_name,
                                 download_digest_sha256);
    }
  }
}

CheckClientDownloadRequest::CheckClientDownloadRequest(
    download::DownloadItem* item,
    CheckDownloadCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : CheckClientDownloadRequestBase(
          item->GetURL(),
          item->GetTargetFilePath(),
          item->GetFullPath(),
          {item->GetTabUrl(), item->GetTabReferrerUrl()},
          content::DownloadItemUtils::GetBrowserContext(item),
          std::move(callback),
          service,
          std::move(database_manager),
          std::move(binary_feature_extractor)),
      item_(item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->AddObserver(this);
  DVLOG(2) << "Starting SafeBrowsing download check for: "
           << item_->DebugString(true);
}

// download::DownloadItem::Observer implementation.
void CheckClientDownloadRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN, REASON_DOWNLOAD_DESTROYED);
}

// static
bool CheckClientDownloadRequest::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path,
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  if (item.GetUrlChain().empty()) {
    *reason = REASON_EMPTY_URL_CHAIN;
    return false;
  }
  const GURL& final_url = item.GetUrlChain().back();
  if (!final_url.is_valid() || final_url.is_empty()) {
    *reason = REASON_INVALID_URL;
    return false;
  }
  if (!final_url.IsStandard() && !final_url.SchemeIsBlob() &&
      !final_url.SchemeIs(url::kDataScheme)) {
    *reason = REASON_UNSUPPORTED_URL_SCHEME;
    return false;
  }
  // TODO(crbug.com/814813): Remove duplicated counting of REMOTE_FILE
  // and LOCAL_FILE in SBClientDownload.UnsupportedScheme.*.
  if (final_url.SchemeIsFile()) {
    *reason = final_url.has_host() ? REASON_REMOTE_FILE : REASON_LOCAL_FILE;
    return false;
  }
  // This check should be last, so we know the earlier checks passed.
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(target_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  *type = download_type_util::GetDownloadType(target_path);
  return true;
}

CheckClientDownloadRequest::~CheckClientDownloadRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  return IsSupportedDownload(*item_, item_->GetTargetFilePath(), reason, type);
}

content::BrowserContext* CheckClientDownloadRequest::GetBrowserContext() {
  return content::DownloadItemUtils::GetBrowserContext(item_);
}

bool CheckClientDownloadRequest::IsCancelled() {
  return item_->GetState() == download::DownloadItem::CANCELLED;
}

void CheckClientDownloadRequest::PopulateRequest(
    ClientDownloadRequest* request) {
  request->mutable_digests()->set_sha256(item_->GetHash());
  request->set_length(item_->GetReceivedBytes());
  for (size_t i = 0; i < item_->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(item_->GetUrlChain()[i]));
    if (i == item_->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(SanitizeUrl(item_->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource->url();
      if (!item_->GetRemoteAddress().empty()) {
        resource->set_remote_ip(item_->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource->referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource->url();
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
  }

  request->set_user_initiated(item_->HasUserGesture());

  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      item_->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
    UMA_HISTOGRAM_COUNTS_100(
        "SafeBrowsing.ReferrerURLChainSize.DownloadAttribution",
        referrer_chain_data->referrer_chain_length());
  }
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckClientDownloadRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckClientDownloadRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service()->client_download_request_callbacks_.Notify(item_, request);
}

void CheckClientDownloadRequest::SetDownloadPingToken(
    const std::string& token) {
  DCHECK(!token.empty());
  DownloadProtectionService::SetDownloadPingToken(item_, token);
}

void CheckClientDownloadRequest::MaybeStorePingsForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  DownloadFeedbackService::MaybeStorePingsForDownload(
      result, upload_requested, item_, request_data, response_body);
}

void CheckClientDownloadRequest::MaybeUploadBinary(
    DownloadCheckResultReason reason) {
  bool upload_for_dlp = ShouldUploadForDlpScan();
  bool upload_for_malware = ShouldUploadForMalwareScan(reason);
  if (upload_for_dlp || upload_for_malware) {
    Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
    if (!profile)
      return;

    const std::string& raw_digest_sha256 = item_->GetHash();
    auto request = std::make_unique<DownloadItemRequest>(
        item_, base::BindOnce(&MaybeReportDownloadDeepScanningVerdict, profile,
                              item_->GetURL(),
                              item_->GetTargetFilePath().AsUTF8Unsafe(),
                              base::HexEncode(raw_digest_sha256.data(),
                                              raw_digest_sha256.size())));

    if (upload_for_dlp) {
      DlpDeepScanningClientRequest dlp_request;
      dlp_request.set_content_source(
          DlpDeepScanningClientRequest::FILE_DOWNLOAD);
      request->set_request_dlp_scan(std::move(dlp_request));
    }

    if (upload_for_malware) {
      MalwareDeepScanningClientRequest malware_request;
      malware_request.set_population(
          MalwareDeepScanningClientRequest::POPULATION_ENTERPRISE);
      malware_request.set_download_token(
          DownloadProtectionService::GetDownloadPingToken(item_));
      request->set_request_malware_scan(std::move(malware_request));
    }

    request->set_dm_token(
        policy::BrowserDMTokenStorage::Get()->RetrieveDMToken());

    service()->UploadForDeepScanning(profile, std::move(request));
  }
}

void CheckClientDownloadRequest::NotifyRequestFinished(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weakptr_factory_.InvalidateWeakPtrs();

  DVLOG(2) << "SafeBrowsing download verdict for: " << item_->DebugString(true)
           << " verdict:" << reason << " result:" << static_cast<int>(result);

  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::ShouldUploadForDlpScan() {
  if (!base::FeatureList::IsEnabled(kDeepScanningOfDownloads))
    return false;

  int check_content_compliance = g_browser_process->local_state()->GetInteger(
      prefs::kCheckContentCompliance);
  if (check_content_compliance !=
          CheckContentComplianceValues::CHECK_DOWNLOADS &&
      check_content_compliance !=
          CheckContentComplianceValues::CHECK_UPLOADS_AND_DOWNLOADS)
    return false;

  // If there's no DM token, the upload will fail, so we can skip uploading now.
  if (policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().empty())
    return false;

  const base::ListValue* domains = g_browser_process->local_state()->GetList(
      prefs::kDomainsToCheckComplianceOfDownloadedContent);
  bool host_in_list =
      std::any_of(domains->GetList().begin(), domains->GetList().end(),
                  [this](const base::Value& domain) {
                    return domain.is_string() &&
                           domain.GetString() == item_->GetURL().host();
                  });

  return host_in_list;
}

bool CheckClientDownloadRequest::ShouldUploadForMalwareScan(
    DownloadCheckResultReason reason) {
  if (!base::FeatureList::IsEnabled(kDeepScanningOfDownloads))
    return false;

  // If we know the file is malicious, we don't need to upload it.
  if (reason != DownloadCheckResultReason::REASON_DOWNLOAD_SAFE &&
      reason != DownloadCheckResultReason::REASON_DOWNLOAD_UNCOMMON &&
      reason != DownloadCheckResultReason::REASON_VERDICT_UNKNOWN)
    return false;

  // This feature can be used to force uploads.
  if (base::FeatureList::IsEnabled(kUploadForMalwareCheck))
    return true;

  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(item_);
  if (!browser_context)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  int send_files_for_malware_check = profile->GetPrefs()->GetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck);
  if (send_files_for_malware_check !=
      SendFilesForMalwareCheckValues::SEND_DOWNLOADS)
    return false;

  // If there's no DM token, the upload will fail, so we can skip uploading now.
  if (policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().empty())
    return false;

  return true;
}

}  // namespace safe_browsing
