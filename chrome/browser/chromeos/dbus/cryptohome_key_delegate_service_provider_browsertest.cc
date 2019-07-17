// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/dbus/cryptohome_key_delegate_service_provider.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/constants/cryptohome_key_delegate_constants.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "extensions/browser/deferred_start_render_host.h"
#include "extensions/browser/deferred_start_render_host_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace {

// Extracts the SubjectPublicKeyInfo from the given certificate.
std::string GetCertSpki(const net::X509Certificate& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return spki_bytes.as_string();
}

// Observer that allows to wait until the extension's background page was first
// loaded.
// TODO(crbug.com/826417): Extract into a generic helper to be usable in other
// tests.
class ExtensionBackgroundPageFirstLoadObserver final
    : public extensions::ProcessManagerObserver,
      public extensions::DeferredStartRenderHostObserver {
 public:
  ExtensionBackgroundPageFirstLoadObserver(
      content::BrowserContext* browser_context,
      const std::string& extension_id)
      : extension_id_(extension_id),
        process_manager_(extensions::ProcessManager::Get(browser_context)) {
    process_manager_->AddObserver(this);
    extension_host_ =
        process_manager_->GetBackgroundHostForExtension(extension_id_);
    if (extension_host_)
      OnObtainedExtensionHost();
  }

  ~ExtensionBackgroundPageFirstLoadObserver() override {
    if (extension_host_) {
      static_cast<extensions::DeferredStartRenderHost*>(extension_host_)
          ->RemoveDeferredStartRenderHostObserver(this);
    }
    process_manager_->RemoveObserver(this);
  }

  void Wait() {
    if (!extension_host_ || !extension_host_->has_loaded_once())
      run_loop_.Run();
  }

 private:
  // extensions::ProcessManagerObserver:
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override {
    if (host->extension_id() == extension_id_) {
      DCHECK(!extension_host_);
      extension_host_ = host;
      OnObtainedExtensionHost();
    }
  }

  // extensions::DeferredStartRenderHostObserver:
  void OnDeferredStartRenderHostDidStopFirstLoad(
      const extensions::DeferredStartRenderHost* /* host */) override {
    run_loop_.Quit();
  }

  void OnObtainedExtensionHost() {
    static_cast<extensions::DeferredStartRenderHost*>(extension_host_)
        ->AddDeferredStartRenderHostObserver(this);
  }

  const std::string extension_id_;
  extensions::ProcessManager* const process_manager_;
  extensions::ExtensionHost* extension_host_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBackgroundPageFirstLoadObserver);
};

}  // namespace

// Tests for the CryptohomeKeyDelegateServiceProvider class.
class CryptohomeKeyDelegateServiceProviderTest
    : public policy::SigninProfileExtensionsPolicyTestBase {
 protected:
  CryptohomeKeyDelegateServiceProviderTest()
      : policy::SigninProfileExtensionsPolicyTestBase(
            version_info::Channel::UNKNOWN) {}

  ~CryptohomeKeyDelegateServiceProviderTest() override {}

  void SetUpOnMainThread() override {
    policy::SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread();

    dbus_service_test_helper_ =
        std::make_unique<chromeos::ServiceProviderTestHelper>();
    dbus_service_test_helper_->SetUp(
        cryptohome::kCryptohomeKeyDelegateServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeKeyDelegateServicePath),
        cryptohome::kCryptohomeKeyDelegateInterface,
        cryptohome::
            kCryptohomeKeyDelegateChallengeKey /* exported_method_name */,
        &service_provider_);

    PrepareExtension();
  }

  void TearDownOnMainThread() override {
    dbus_service_test_helper_->TearDown();
    dbus_service_test_helper_.reset();

    cert_provider_extension_.reset();

    policy::SigninProfileExtensionsPolicyTestBase::TearDownOnMainThread();
  }

  // Refreshes the browser's state from the current certificate providers.
  void RefreshCertsFromCertProviders() {
    chromeos::CertificateProviderService* cert_provider_service =
        chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
            GetInitialProfile());
    std::unique_ptr<chromeos::CertificateProvider> cert_provider =
        cert_provider_service->CreateCertificateProvider();
    base::RunLoop run_loop;
    cert_provider->GetCertificates(base::BindLambdaForTesting(
        [&](net::ClientCertIdentityList) { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Calls the tested ChallengeKey D-Bus method, requesting a signature
  // challenge.
  // Fills |signature| with the data returned by the call.
  void CallSignatureChallengeKey(std::vector<uint8_t>* signature) {
    const cryptohome::AccountIdentifier account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(EmptyAccountId());
    cryptohome::KeyChallengeRequest request;
    request.set_challenge_type(
        cryptohome::KeyChallengeRequest::CHALLENGE_TYPE_SIGNATURE);
    request.mutable_signature_request_data()->set_data_to_sign(kDataToSign);
    request.mutable_signature_request_data()->set_public_key_spki_der(
        GetCertSpki(*cert_provider_extension_->certificate()));
    request.mutable_signature_request_data()->set_signature_algorithm(
        cryptohome::CHALLENGE_RSASSA_PKCS1_V1_5_SHA256);

    dbus::MethodCall method_call(
        cryptohome::kCryptohomeKeyDelegateInterface,
        cryptohome::kCryptohomeKeyDelegateChallengeKey);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(account_identifier);
    writer.AppendProtoAsArrayOfBytes(request);

    std::unique_ptr<dbus::Response> dbus_response =
        dbus_service_test_helper_->CallMethod(&method_call);

    dbus::MessageReader reader(dbus_response.get());
    cryptohome::KeyChallengeResponse response;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&response));
    EXPECT_TRUE(response.has_signature_response_data());
    EXPECT_TRUE(response.signature_response_data().has_signature());
    signature->assign(response.signature_response_data().signature().begin(),
                      response.signature_response_data().signature().end());
  }

  // Returns whether the given |signature| is a valid signature of the original
  // data.
  bool IsSignatureValid(const std::vector<uint8_t>& signature) const {
    const std::string spki =
        GetCertSpki(*cert_provider_extension_->certificate());
    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                             signature,
                             base::as_bytes(base::make_span(spki)))) {
      return false;
    }
    verifier.VerifyUpdate(base::as_bytes(base::make_span(kDataToSign)));
    return verifier.VerifyFinal();
  }

 private:
  // Test certificate provider extension:
  const std::string kExtensionId = "ecmhnokcdiianioonpgakiooenfnonid";
  const std::string kExtensionUpdateManifestPath =
      "/extensions/test_certificate_provider/update_manifest.xml";

  // Data that is passed as an input for the signature challenge request.
  const std::string kDataToSign = "some_data";

  // Installs the certificate provider extension into the sign-in profile.
  void PrepareExtension() {
    Profile* const profile = GetInitialProfile();
    cert_provider_extension_ =
        std::make_unique<TestCertificateProviderExtension>(profile,
                                                           kExtensionId);
    ExtensionBackgroundPageFirstLoadObserver bg_page_first_load_observer(
        profile, kExtensionId);
    AddExtensionForForceInstallation(kExtensionId,
                                     kExtensionUpdateManifestPath);
    bg_page_first_load_observer.Wait();
  }

  chromeos::CryptohomeKeyDelegateServiceProvider service_provider_;
  std::unique_ptr<chromeos::ServiceProviderTestHelper>
      dbus_service_test_helper_;
  std::unique_ptr<TestCertificateProviderExtension> cert_provider_extension_;
};

// Verifies that the ChallengeKey method handles the signature challenge request
// by passing the request to the certificate provider extension.
IN_PROC_BROWSER_TEST_F(CryptohomeKeyDelegateServiceProviderTest,
                       SignatureBasic) {
  // Populate the browser's state with the mapping between the test certificate
  // provider extension and the certs that it provides, so that the tested
  // implementation knows where it should send challenges to. In the real-world
  // usage, this step is done by the Login/Lock Screens while preparing the
  // parameters that are later used by the cryptohomed daemon for calling our
  // D-Bus service.
  RefreshCertsFromCertProviders();

  std::vector<uint8_t> signature;
  CallSignatureChallengeKey(&signature);
  EXPECT_TRUE(IsSignatureValid(signature));
}
