// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/ssl/certificate_error_report.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"

namespace {

// Certificate reports are only sent from official builds, but this flag can be
// set by tests.
bool g_is_fake_official_build_for_cert_verifier_testing = false;

}  // namespace

TrialComparisonCertVerifierController::TrialComparisonCertVerifierController(
    Profile* profile)
    : profile_(profile) {
  if (!MaybeAllowedForProfile(profile_)) {
    // Don't bother registering pref change notifier if the trial could never be
    // enabled.
    return;
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::BindRepeating(&TrialComparisonCertVerifierController::RefreshState,
                          base::Unretained(this)));
}

TrialComparisonCertVerifierController::
    ~TrialComparisonCertVerifierController() = default;

// static
bool TrialComparisonCertVerifierController::MaybeAllowedForProfile(
    Profile* profile) {
  bool is_official_build = g_is_fake_official_build_for_cert_verifier_testing;
#if defined(OFFICIAL_BUILD) && defined(GOOGLE_CHROME_BUILD)
  is_official_build = true;
#endif

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  // If the builtin verifier is enabled as the default verifier, the trial does
  // not make sense.
  if (base::FeatureList::IsEnabled(net::features::kCertVerifierBuiltinFeature))
    return false;
#endif

  return is_official_build &&
         base::FeatureList::IsEnabled(
             features::kCertDualVerificationTrialFeature) &&
         !profile->IsOffTheRecord();
}

void TrialComparisonCertVerifierController::AddClient(
    network::mojom::TrialComparisonCertVerifierConfigClientPtr config_client,
    network::mojom::TrialComparisonCertVerifierReportClientRequest
        report_client_request) {
  binding_set_.AddBinding(this, std::move(report_client_request));
  config_client_set_.AddPtr(std::move(config_client));
}

bool TrialComparisonCertVerifierController::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only allow on non-incognito profiles which have SBER opt-in set.
  // See design doc for more details:
  // https://docs.google.com/document/d/1AM1CD42bC6LHWjKg-Hkid_RLr2DH6OMzstH9-pGSi-g

  if (!MaybeAllowedForProfile(profile_))
    return false;

  const PrefService& prefs = *profile_->GetPrefs();

  return safe_browsing::IsExtendedReportingEnabled(prefs);
}

void TrialComparisonCertVerifierController::SendTrialReport(
    const std::string& hostname,
    const scoped_refptr<net::X509Certificate>& unverified_cert,
    bool enable_rev_checking,
    bool require_rev_checking_local_anchors,
    bool enable_sha1_local_anchors,
    bool disable_symantec_enforcement,
    const net::CertVerifyResult& primary_result,
    const net::CertVerifyResult& trial_result) {
  if (!IsAllowed() ||
      base::GetFieldTrialParamByFeatureAsBool(
          features::kCertDualVerificationTrialFeature, "uma_only", false)) {
    return;
  }

  CertificateErrorReport report(
      hostname, *unverified_cert, enable_rev_checking,
      require_rev_checking_local_anchors, enable_sha1_local_anchors,
      disable_symantec_enforcement, primary_result, trial_result);

  report.AddNetworkTimeInfo(g_browser_process->network_time_tracker());
  report.AddChromeChannel(chrome::GetChannel());

  std::string serialized_report;
  if (!report.Serialize(&serialized_report))
    return;

  CertificateReportingServiceFactory::GetForBrowserContext(profile_)->Send(
      serialized_report);
}

// static
void TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
    bool fake_official_build) {
  g_is_fake_official_build_for_cert_verifier_testing = fake_official_build;
}

void TrialComparisonCertVerifierController::RefreshState() {
  const bool is_allowed = IsAllowed();
  config_client_set_.ForAllPtrs(
      [is_allowed](
          network::mojom::TrialComparisonCertVerifierConfigClient* client) {
        client->OnTrialConfigUpdated(is_allowed);
      });
}
