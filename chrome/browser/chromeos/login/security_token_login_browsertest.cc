// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension_login_screen_mixin.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {

namespace {

// The PIN code that the test certificate provider extension is configured to
// expect.
constexpr char kCorrectPin[] = "17093";

constexpr char kChallengeData[] = "challenge";

// Custom implementation of the CryptohomeClient that triggers the
// challenge-response protocol when authenticating the user.
class ChallengeResponseFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  ChallengeResponseFakeCryptohomeClient() = default;
  ChallengeResponseFakeCryptohomeClient(
      const ChallengeResponseFakeCryptohomeClient&) = delete;
  ChallengeResponseFakeCryptohomeClient& operator=(
      const ChallengeResponseFakeCryptohomeClient&) = delete;
  ~ChallengeResponseFakeCryptohomeClient() override = default;

  void set_challenge_response_account_id(const AccountId& account_id) {
    challenge_response_account_id_ = account_id;
  }

  void MountEx(const cryptohome::AccountIdentifier& cryptohome_id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override {
    Profile* signin_profile = ProfileHelper::GetSigninProfile();
    CertificateProviderService* certificate_provider_service =
        CertificateProviderServiceFactory::GetForBrowserContext(signin_profile);
    // Note: The real cryptohome would call the "ChallengeKey" D-Bus method
    // exposed by Chrome via org.chromium.CryptohomeKeyDelegateInterface, but
    // we're directly requesting the extension in order to avoid extra
    // complexity in this UI-oriented browser test.
    certificate_provider_service->RequestSignatureBySpki(
        TestCertificateProviderExtension::GetCertificateSpki(),
        SSL_SIGN_RSA_PKCS1_SHA256,
        base::as_bytes(base::make_span(kChallengeData)),
        challenge_response_account_id_,
        base::BindOnce(&ChallengeResponseFakeCryptohomeClient::
                           ContinueMountExWithSignature,
                       base::Unretained(this), cryptohome_id,
                       std::move(callback)));
  }

 private:
  void ContinueMountExWithSignature(
      const cryptohome::AccountIdentifier& cryptohome_id,
      DBusMethodCallback<cryptohome::BaseReply> callback,
      net::Error error,
      const std::vector<uint8_t>& signature) {
    cryptohome::BaseReply reply;
    cryptohome::MountReply* mount =
        reply.MutableExtension(cryptohome::MountReply::reply);
    mount->set_sanitized_username(GetStubSanitizedUsername(cryptohome_id));
    if (error != net::OK || signature.empty())
      reply.set_error(cryptohome::CRYPTOHOME_ERROR_MOUNT_FATAL);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), reply));
  }

  AccountId challenge_response_account_id_;
};

}  // namespace

// Tests the challenge-response based login (e.g., using a smart card) for an
// existing user.
class SecurityTokenLoginTest : public MixinBasedInProcessBrowserTest,
                               public LocalStateMixin::Delegate {
 protected:
  SecurityTokenLoginTest()
      : cryptohome_client_(new ChallengeResponseFakeCryptohomeClient) {
    // Don't shut down when no browser is open, since it breaks the test and
    // since it's not the real Chrome OS behavior.
    set_exit_when_last_browser_closes(false);

    login_manager_mixin_.AppendManagedUsers(1);
    cryptohome_client_->set_challenge_response_account_id(
        GetChallengeResponseAccountId());
  }

  SecurityTokenLoginTest(const SecurityTokenLoginTest&) = delete;
  SecurityTokenLoginTest& operator=(const SecurityTokenLoginTest&) = delete;
  ~SecurityTokenLoginTest() override = default;

  // MixinBasedInProcessBrowserTest:

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);

    // Avoid aborting the user sign-in due to the user policy requests not being
    // faked in the test.
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    cert_provider_extension_mixin_.test_certificate_provider_extension()
        ->set_require_pin(kCorrectPin);
  }

  // LocalStateMixin::Delegate:

  void SetUpLocalState() override { RegisterChallengeResponseKey(); }

  AccountId GetChallengeResponseAccountId() const {
    return login_manager_mixin_.users()[0].account_id;
  }

  void WaitForActiveSession() { login_manager_mixin_.WaitForActiveSession(); }

 private:
  void RegisterChallengeResponseKey() {
    // The global user manager is not created until after the Local State is
    // initialized, but in order for the user_manager::known_user:: methods to
    // work we create a temporary instance of the user manager here.
    auto user_manager = std::make_unique<user_manager::FakeUserManager>();
    user_manager->set_local_state(g_browser_process->local_state());
    user_manager::ScopedUserManager scoper(std::move(user_manager));

    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(
        TestCertificateProviderExtension::GetCertificateSpki());
    challenge_response_key.set_extension_id(
        TestCertificateProviderExtensionLoginScreenMixin::GetExtensionId());

    base::Value challenge_response_keys_value =
        SerializeChallengeResponseKeysForKnownUser({challenge_response_key});
    user_manager::known_user::SetChallengeResponseKeys(
        GetChallengeResponseAccountId(),
        std::move(challenge_response_keys_value));
  }

  // Unowned (referencing a global singleton)
  ChallengeResponseFakeCryptohomeClient* const cryptohome_client_;
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  TestCertificateProviderExtensionLoginScreenMixin
      cert_provider_extension_mixin_{&mixin_host_, &device_state_mixin_,
                                     /*load_extension_immediately=*/true};
};

// TODO(crbug.com/1033936): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_F(SecurityTokenLoginTest, DISABLED_Basic) {
  // The user pod is displayed with the challenge-response "start" button
  // instead of the password input field.
  EXPECT_TRUE(
      ash::LoginScreenTestApi::FocusUser(GetChallengeResponseAccountId()));
  EXPECT_FALSE(ash::LoginScreenTestApi::IsPasswordFieldShown(
      GetChallengeResponseAccountId()));

  // The challenge-response "start" button is clicked.
  base::RunLoop pin_dialog_waiting_run_loop;
  ash::LoginScreenTestApi::SetPinRequestWidgetShownCallback(
      pin_dialog_waiting_run_loop.QuitClosure());
  ash::LoginScreenTestApi::ClickChallengeResponseButton(
      GetChallengeResponseAccountId());

  // The MountEx request is sent to cryptohome, and in turn cryptohome makes a
  // challenge request. The certificate provider extension receives this request
  // and requests the PIN dialog.
  pin_dialog_waiting_run_loop.Run();

  // The PIN is entered.
  ash::LoginScreenTestApi::SubmitPinRequestWidget(kCorrectPin);

  // The PIN is received by the certificate provider extension, which replies to
  // the challenge request. cryptohome receives this response and completes the
  // MountEx request. The user session begins.
  WaitForActiveSession();
}

}  // namespace chromeos
