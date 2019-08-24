// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trial_comparison_cert_verifier_mojo.h"

#include <utility>

#include "build/build_config.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/trial_comparison_cert_verifier.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "net/cert/internal/trust_store_mac.h"
#endif

namespace network {

TrialComparisonCertVerifierMojo::TrialComparisonCertVerifierMojo(
    bool initial_allowed,
    mojom::TrialComparisonCertVerifierConfigClientRequest config_client_request,
    mojom::TrialComparisonCertVerifierReportClientPtrInfo report_client,
    scoped_refptr<net::CertVerifyProc> primary_verify_proc,
    scoped_refptr<net::CertVerifyProc> trial_verify_proc)
    : binding_(this, std::move(config_client_request)),
      report_client_(std::move(report_client)) {
  trial_comparison_cert_verifier_ =
      std::make_unique<net::TrialComparisonCertVerifier>(
          initial_allowed, primary_verify_proc, trial_verify_proc,
          base::BindRepeating(
              &TrialComparisonCertVerifierMojo::OnSendTrialReport,
              // Unretained safe because the report_callback will not be called
              // after trial_comparison_cert_verifier_ is destroyed.
              base::Unretained(this)));
}

TrialComparisonCertVerifierMojo::~TrialComparisonCertVerifierMojo() = default;

int TrialComparisonCertVerifierMojo::Verify(
    const RequestParams& params,
    net::CertVerifyResult* verify_result,
    net::CompletionOnceCallback callback,
    std::unique_ptr<Request>* out_req,
    const net::NetLogWithSource& net_log) {
  return trial_comparison_cert_verifier_->Verify(
      params, verify_result, std::move(callback), out_req, net_log);
}

void TrialComparisonCertVerifierMojo::SetConfig(const Config& config) {
  trial_comparison_cert_verifier_->SetConfig(config);
}

void TrialComparisonCertVerifierMojo::OnTrialConfigUpdated(bool allowed) {
  trial_comparison_cert_verifier_->set_trial_allowed(allowed);
}

void TrialComparisonCertVerifierMojo::OnSendTrialReport(
    const std::string& hostname,
    const scoped_refptr<net::X509Certificate>& unverified_cert,
    bool enable_rev_checking,
    bool require_rev_checking_local_anchors,
    bool enable_sha1_local_anchors,
    bool disable_symantec_enforcement,
    const net::CertVerifyResult& primary_result,
    const net::CertVerifyResult& trial_result) {
  network::mojom::CertVerifierDebugInfoPtr debug_info =
      network::mojom::CertVerifierDebugInfo::New();
#if defined(OS_MACOSX) && !defined(OS_IOS)
  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::Get(&trial_result);
  if (mac_trust_debug_info) {
    debug_info->mac_combined_trust_debug_info =
        mac_trust_debug_info->combined_trust_debug_info();
  }
#endif

  report_client_->SendTrialReport(
      hostname, unverified_cert, enable_rev_checking,
      require_rev_checking_local_anchors, enable_sha1_local_anchors,
      disable_symantec_enforcement, primary_result, trial_result,
      std::move(debug_info));
}

}  // namespace network
