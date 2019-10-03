// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trial_comparison_cert_verifier_mojo.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "net/cert/internal/trust_store_mac.h"
#endif

struct ReceivedReport {
  std::string hostname;
  scoped_refptr<net::X509Certificate> unverified_cert;
  bool enable_rev_checking;
  bool require_rev_checking_local_anchors;
  bool enable_sha1_local_anchors;
  bool disable_symantec_enforcement;
  net::CertVerifyResult primary_result;
  net::CertVerifyResult trial_result;
  network::mojom::CertVerifierDebugInfoPtr debug_info;
};

class FakeReportClient
    : public network::mojom::TrialComparisonCertVerifierReportClient {
 public:
  explicit FakeReportClient(
      network::mojom::TrialComparisonCertVerifierReportClientRequest
          report_client_request)
      : binding_(this, std::move(report_client_request)) {}

  // TrialComparisonCertVerifierReportClient implementation:
  void SendTrialReport(
      const std::string& hostname,
      const scoped_refptr<net::X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result,
      network::mojom::CertVerifierDebugInfoPtr debug_info) override {
    ReceivedReport report;
    report.hostname = hostname;
    report.unverified_cert = unverified_cert;
    report.enable_rev_checking = enable_rev_checking;
    report.require_rev_checking_local_anchors =
        require_rev_checking_local_anchors;
    report.enable_sha1_local_anchors = enable_sha1_local_anchors;
    report.disable_symantec_enforcement = disable_symantec_enforcement;
    report.primary_result = primary_result;
    report.trial_result = trial_result;
    report.debug_info = std::move(debug_info);
    reports_.push_back(std::move(report));

    run_loop_.Quit();
  }

  const std::vector<ReceivedReport>& reports() const { return reports_; }

  void WaitForReport() { run_loop_.Run(); }

 private:
  mojo::Binding<network::mojom::TrialComparisonCertVerifierReportClient>
      binding_;

  std::vector<ReceivedReport> reports_;
  base::RunLoop run_loop_;
};

TEST(TrialComparisonCertVerifierMojoTest, SendReportDebugInfo) {
  base::test::TaskEnvironment scoped_task_environment;

  scoped_refptr<net::X509Certificate> unverified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  scoped_refptr<net::X509Certificate> chain1 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "x509_verify_results.chain.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  scoped_refptr<net::X509Certificate> chain2 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "multi-root-chain1.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = chain1;
  net::CertVerifyResult trial_result;
  trial_result.verified_cert = chain2;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  constexpr int kExpectedTrustDebugInfo = 0xABCD;
  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::GetOrCreate(&trial_result);
  ASSERT_TRUE(mac_trust_debug_info);
  mac_trust_debug_info->UpdateTrustDebugInfo(kExpectedTrustDebugInfo);
#endif

  base::Time time = base::Time::Now();
  net::der::GeneralizedTime der_time;
  der_time.year = 2019;
  der_time.month = 9;
  der_time.day = 27;
  der_time.hours = 22;
  der_time.minutes = 11;
  der_time.seconds = 8;
  net::CertVerifyProcBuiltinResultDebugData::Create(&trial_result, time,
                                                    der_time);

  network::mojom::TrialComparisonCertVerifierReportClientPtrInfo
      report_client_ptr;
  FakeReportClient report_client(mojo::MakeRequest(&report_client_ptr));
  network::TrialComparisonCertVerifierMojo tccvm(
      true, {}, std::move(report_client_ptr), nullptr, nullptr);

  tccvm.OnSendTrialReport("example.com", unverified_cert, false, false, false,
                          false, primary_result, trial_result);

  report_client.WaitForReport();

  ASSERT_EQ(1U, report_client.reports().size());
  const ReceivedReport& report = report_client.reports()[0];
  EXPECT_TRUE(
      unverified_cert->EqualsIncludingChain(report.unverified_cert.get()));
  EXPECT_TRUE(
      chain1->EqualsIncludingChain(report.primary_result.verified_cert.get()));
  EXPECT_TRUE(
      chain2->EqualsIncludingChain(report.trial_result.verified_cert.get()));

  ASSERT_TRUE(report.debug_info);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  EXPECT_EQ(kExpectedTrustDebugInfo,
            report.debug_info->mac_combined_trust_debug_info);
#endif

  EXPECT_EQ(time, report.debug_info->trial_verification_time);
  EXPECT_EQ("20190927221108Z", report.debug_info->trial_der_verification_time);
}
