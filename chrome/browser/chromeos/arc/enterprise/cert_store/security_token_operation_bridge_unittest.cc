// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "base//strings/string_piece.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/security_token_operation_bridge.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_info.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace arc {
namespace keymaster {

constexpr char kCertFileName[] = "client_1.pem";
constexpr char kExtensionId[] = "extension";

namespace {

void ReplyToSignRequest(chromeos::CertificateProviderService* service,
                        int sign_request_id,
                        const std::vector<uint8_t>* signature) {
  service->ReplyToSignRequest(kExtensionId, sign_request_id, *signature);
}

class FakeDelegate : public chromeos::CertificateProviderService::Delegate {
 public:
  // * |cert_file_name| - should be empty if there is no certificates.
  // * |expected_signature| - should be empty to cause an error.
  FakeDelegate(chromeos::CertificateProviderService* service,
               const std::string& cert_file_name,
               const std::vector<uint8_t> expected_signature)
      : service_(service),
        cert_file_name_(cert_file_name),
        expected_signature_(expected_signature) {}

  std::vector<std::string> CertificateProviderExtensions() override {
    return std::vector<std::string>{kExtensionId};
  }

  void BroadcastCertificateRequest(int cert_request_id) override {
    chromeos::certificate_provider::CertificateInfoList infos;
    if (!cert_file_name_.empty()) {
      chromeos::certificate_provider::CertificateInfo cert_info;
      cert_info.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), cert_file_name_);
      EXPECT_TRUE(cert_info.certificate)
          << "Could not load " << cert_file_name_;
      cert_info.supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);
      infos.push_back(cert_info);
    }
    service_->SetCertificatesProvidedByExtension(kExtensionId, cert_request_id,
                                                 infos);
  }

  bool DispatchSignRequestToExtension(
      const std::string& extension_id,
      int sign_request_id,
      uint16_t algorithm,
      const scoped_refptr<net::X509Certificate>& certificate,
      base::span<const uint8_t> digest) override {
    EXPECT_EQ(kExtensionId, extension_id);

    // Reply after the method result is returned.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ReplyToSignRequest, service_,
                                  sign_request_id, &expected_signature_));
    return true;
  }

 private:
  chromeos::CertificateProviderService* service_;  // Not owned.
  std::string cert_file_name_;
  std::vector<uint8_t> expected_signature_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

}  // namespace

class SecurityTokenOperationBridgeTest
    : public testing::TestWithParam<std::tuple<mojom::SignatureResult,
                                               std::string,
                                               std::vector<uint8_t>>> {
 public:
  SecurityTokenOperationBridgeTest() = default;
  SecurityTokenOperationBridgeTest(const SecurityTokenOperationBridgeTest&) =
      delete;
  SecurityTokenOperationBridgeTest& operator=(
      const SecurityTokenOperationBridgeTest&) = delete;

  void SetUp() override {
    certificate_provider_service_.SetDelegate(
        std::make_unique<FakeDelegate>(&certificate_provider_service_,
                                       cert_file_name(), expected_signature()));
    bridge_ = std::make_unique<SecurityTokenOperationBridge>(
        &certificate_provider_service_);

    // Request available certificates.
    auto certificate_provider =
        certificate_provider_service()->CreateCertificateProvider();
    base::RunLoop run_loop;
    certificate_provider->GetCertificates(base::BindOnce(
        [](base::RepeatingClosure quit_closure,
           net::ClientCertIdentityList certs) { quit_closure.Run(); },
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TearDown() override { bridge_ = nullptr; }

  chromeos::CertificateProviderService* certificate_provider_service() {
    return &certificate_provider_service_;
  }

  SecurityTokenOperationBridge* bridge() { return bridge_.get(); }
  mojom::SignatureResult expected_result() { return std::get<0>(GetParam()); }
  std::string cert_file_name() { return std::get<1>(GetParam()); }
  std::vector<uint8_t> expected_signature() { return std::get<2>(GetParam()); }

  // Extract SPKI from certificate stored in |kCertFileName|.
  std::string spki() {
    auto certificate =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    base::StringPiece spki_bytes;
    if (!net::asn1::ExtractSPKIFromDERCert(
            net::x509_util::CryptoBufferAsStringPiece(
                certificate->cert_buffer()),
            &spki_bytes)) {
      return "";
    }
    return spki_bytes.as_string();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  chromeos::CertificateProviderService certificate_provider_service_;
  std::unique_ptr<SecurityTokenOperationBridge> bridge_;
};

TEST_P(SecurityTokenOperationBridgeTest, BasicTest) {
  base::RunLoop run_loop;

  bridge()->SignDigest(
      spki(), mojom::Algorithm::kRsaPkcs1, mojom::Digest::kSha256,
      std::vector<uint8_t>(),
      base::BindOnce(
          [](mojom::SignatureResult expected_result,
             const std::vector<uint8_t>& expected_signature,
             base::RepeatingClosure quit_closure, mojom::SignatureResult result,
             const base::Optional<std::vector<uint8_t>>& signature) {
            quit_closure.Run();
            EXPECT_EQ(expected_result, result);
            if (result == mojom::SignatureResult::kOk) {
              EXPECT_EQ(expected_signature, signature);
            } else {
              EXPECT_FALSE(signature.has_value());
            }
          },
          expected_result(), expected_signature(), run_loop.QuitClosure()));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    SecurityTokenOperationBridgeTest,
    SecurityTokenOperationBridgeTest,
    ::testing::Values(std::make_tuple(mojom::SignatureResult::kOk,
                                      kCertFileName,
                                      std::vector<uint8_t>(1)),
                      std::make_tuple(mojom::SignatureResult::kFailed,
                                      kCertFileName,
                                      std::vector<uint8_t>()),
                      std::make_tuple(mojom::SignatureResult::kFailed,
                                      "",
                                      std::vector<uint8_t>(1))));
}  // namespace keymaster
}  // namespace arc
