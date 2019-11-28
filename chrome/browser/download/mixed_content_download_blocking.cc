// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/mixed_content_download_blocking.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/download/public/common/download_stats.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using download::DownloadSource;

namespace {

// Map the string file extension to the corresponding histogram enum.
InsecureDownloadExtensions GetExtensionEnumFromString(
    const std::string& extension) {
  if (extension.empty())
    return InsecureDownloadExtensions::kNone;

  auto lower_extension = base::ToLowerASCII(extension);
  for (auto candidate : kExtensionsToEnum) {
    if (candidate.extension == lower_extension)
      return candidate.value;
  }
  return InsecureDownloadExtensions::kUnknown;
}

// Get the appropriate histogram metric name for the initiator/download security
// state combo.
std::string GetDownloadBlockingExtensionMetricName(
    InsecureDownloadSecurityStatus status) {
  switch (status) {
    case InsecureDownloadSecurityStatus::kInitiatorUnknownFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorUnknown,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorUnknownFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorUnknown,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorSecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorSecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInsecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInsecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInsecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredSecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredSecure,
          kInsecureDownloadHistogramTargetInsecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredInsecureFileSecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredInsecure,
          kInsecureDownloadHistogramTargetSecure);
    case InsecureDownloadSecurityStatus::kInitiatorInferredInsecureFileInsecure:
      return GetDLBlockingHistogramName(
          kInsecureDownloadExtensionInitiatorInferredInsecure,
          kInsecureDownloadHistogramTargetInsecure);
  }
  NOTREACHED();
  return std::string();
}

// Get appropriate enum value for the initiator/download security state combo
// for histogram reporting. |dl_secure| signifies whether the download was
// a secure source. |inferred| is whether the initiator value is our best guess.
InsecureDownloadSecurityStatus GetDownloadBlockingEnum(
    base::Optional<url::Origin> initiator,
    bool dl_secure,
    bool inferred) {
  if (inferred) {
    if (initiator->GetURL().SchemeIsCryptographic()) {
      if (dl_secure) {
        return InsecureDownloadSecurityStatus::
            kInitiatorInferredSecureFileSecure;
      }
      return InsecureDownloadSecurityStatus::
          kInitiatorInferredSecureFileInsecure;
    }

    if (dl_secure) {
      return InsecureDownloadSecurityStatus::
          kInitiatorInferredInsecureFileSecure;
    }
    return InsecureDownloadSecurityStatus::
        kInitiatorInferredInsecureFileInsecure;
  }

  if (!initiator.has_value()) {
    if (dl_secure)
      return InsecureDownloadSecurityStatus::kInitiatorUnknownFileSecure;
    return InsecureDownloadSecurityStatus::kInitiatorUnknownFileInsecure;
  }

  if (initiator->GetURL().SchemeIsCryptographic()) {
    if (dl_secure)
      return InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure;
    return InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure;
  }

  if (dl_secure)
    return InsecureDownloadSecurityStatus::kInitiatorInsecureFileSecure;
  return InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure;
}

// Dump without crashing because of the given |issue|.
void ReportTrace(const std::string& issue) {
  LOG(WARNING) << "Unexpected download configuration:" << issue;
  static auto* initiator_problem_reason = base::debug::AllocateCrashKeyString(
      "download_initiator_problem_reason", base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(initiator_problem_reason, issue);
  base::debug::DumpWithoutCrashing();
}

}  // namespace

bool ShouldBlockFileAsMixedContent(const base::FilePath& path,
                                   const download::DownloadItem& item) {
  // Extensions must be in lower case! Extensions are compared against save path
  // determined by Chrome prior to the user seeing a file picker.
  const std::vector<std::string> kDefaultUnsafeExtensions = {
      "exe", "scr", "msi", "vb",  "dmg", "pkg", "crx",
      "gz",  "zip", "bz2", "rar", "7z",  "tar",
  };

  // Evaluate download security
  const GURL& dl_url = item.GetURL();
  bool is_download_secure = content::IsOriginSecure(dl_url) ||
                            dl_url.SchemeIsBlob() || dl_url.SchemeIsFile();
  bool is_redirect_chain_secure = true;
  for (const auto& url : item.GetUrlChain()) {
    if (!content::IsOriginSecure(url)) {
      is_redirect_chain_secure = false;
      break;
    }
  }
  is_download_secure = is_download_secure && is_redirect_chain_secure;

  // Check field trials for override of the unsafe extensions.
  std::string field_trial_arg = base::GetFieldTrialParamValueByFeature(
      features::kTreatUnsafeDownloadsAsActive,
      features::kTreatUnsafeDownloadsAsActiveParamName);
  std::vector<base::StringPiece> unsafe_extensions = base::SplitStringPiece(
      field_trial_arg, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (unsafe_extensions.empty()) {
    unsafe_extensions.insert(unsafe_extensions.end(),
                             kDefaultUnsafeExtensions.begin(),
                             kDefaultUnsafeExtensions.end());
  }

  auto initiator = item.GetRequestInitiator();

  bool is_inferred = false;
  if (!initiator.has_value() && item.GetTabUrl().is_valid()) {
    initiator = url::Origin::Create(item.GetTabUrl());
    is_inferred = true;
  }

  auto download_source = item.GetDownloadSource();
  auto transition_type = item.GetTransitionType();

  // Report a trace if we inferred an initiator in an unexpected way. We expect
  // this to happen with:
  //  - context-menu-initiated downloads (CONTEXT_MENU),
  //  - context-menu-inititated page-saves (WEB_CONTENTS_API),
  //  - extension download API,
  //  - extension installs,
  //  - Chrome-initiated top-level navigations (devtools, 'back to safety',
  //    chrome app installs, session restore, a million more). It's a little
  //    weird though that these trigger downloads often...
  //  - reloads (since blocked downloads won't give a chance to reload anyway),
  //  - anything triggered directly from the address bar.
  //  - user-initiated downloads of offline pages on Android,
  //  - requests in e.g., webview/CCT.
  //
  // TODO(1029082): We also occasionally find 'regular' navigations without
  // initiators (NAVIGATION/PAGE_TRANSITION_LINK).  The trace doesn't help in
  // this case, so ignore them here.
  if (is_inferred && download_source != DownloadSource::CONTEXT_MENU &&
      download_source != DownloadSource::WEB_CONTENTS_API &&
      download_source != DownloadSource::EXTENSION_API &&
      download_source != DownloadSource::EXTENSION_INSTALLER &&
      download_source != DownloadSource::OFFLINE_PAGE &&
      !(transition_type & ui::PAGE_TRANSITION_AUTO_TOPLEVEL) &&
      !(transition_type & ui::PAGE_TRANSITION_RELOAD) &&
      !(transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) &&
      !(transition_type & ui::PAGE_TRANSITION_FROM_API) &&
      !(download_source == DownloadSource::NAVIGATION &&
        PageTransitionTypeIncludingQualifiersIs(transition_type,
                                                ui::PAGE_TRANSITION_LINK))) {
    ReportTrace(base::StringPrintf("inferred initiator [%d, %x]",
                                   download_source, transition_type));
  }

  // Report a trace if we still don't have an initiator. This mostly happens
  // with INTERNAL_API calls, which are OK.
  //
  // TODO(1029062): INTERNAL_API is also used for background fetch. That
  // probably isn't the correct behavior, since INTERNAL_API is otherwise used
  // for Chrome stuff. Background fetch should probably be HTTPS-only.
  if (!initiator.has_value() &&
      download_source != DownloadSource::INTERNAL_API) {
    ReportTrace(base::StringPrintf("no initiator found [%d, %x]",
                                   download_source, transition_type));
  }

  // Then see if that extension is blocked
  bool found_blocked_extension = false;
#if defined(OS_WIN)
  std::string extension(base::WideToUTF8(path.FinalExtension()));
#else
  std::string extension(path.FinalExtension());
#endif
  if (!extension.empty()) {
    extension = extension.substr(1);  // Omit leading dot.
    for (const auto& unsafe_extension : unsafe_extensions) {
      if (base::LowerCaseEqualsASCII(extension, unsafe_extension)) {
        found_blocked_extension = true;
        break;
      }
    }
  }

  auto security_status =
      GetDownloadBlockingEnum(initiator, is_download_secure, is_inferred);
  base::UmaHistogramEnumeration(
      GetDownloadBlockingExtensionMetricName(security_status),
      GetExtensionEnumFromString(extension));
  base::UmaHistogramEnumeration(kInsecureDownloadHistogramName,
                                security_status);
  download::RecordDownloadValidationMetrics(
      download::DownloadMetricsCallsite::kMixContentDownloadBlocking,
      download::CheckDownloadConnectionSecurity(dl_url, item.GetUrlChain()),
      download::DownloadContentFromMimeType(item.GetMimeType(), false));

  if (!(initiator.has_value() && initiator->GetURL().SchemeIsCryptographic() &&
        !is_download_secure && found_blocked_extension &&
        base::FeatureList::IsEnabled(
            features::kTreatUnsafeDownloadsAsActive))) {
    return false;
  }

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(&item);
  if (web_contents) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Mixed Content: The site at '%s' was loaded over a secure "
            "connection, but the file at '%s' was %s an insecure "
            "connection. This file should be served over HTTPS.",
            initiator->GetURL().spec().c_str(), item.GetURL().spec().c_str(),
            (is_redirect_chain_secure ? "loaded over" : "redirected through")));
  }

  return true;
}
