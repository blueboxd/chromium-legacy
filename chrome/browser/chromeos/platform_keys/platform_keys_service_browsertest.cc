// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace platform_keys {
namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";

enum class ProfileToUse {
  // A Profile that belongs to a user that is not affiliated with the device (no
  // access to the system token).
  kUnaffiliatedUserProfile,
  // A Profile that belongs to a user that is affiliated with the device (access
  // to the system token).
  kAffiliatedUserProfile,
  // The sign-in screen profile.
  kSigninProfile
};

// Describes a test configuration for the test suite.
struct TestConfig {
  // The profile for which PlatformKeysService should be tested.
  ProfileToUse profile_to_use;

  // The token IDs that are expected to be available. This will be checked by
  // the GetTokens test, and operation for these tokens will be performed by the
  // other tests.
  std::vector<std::string> token_ids;
};

// A helper that waits until execution of an asynchronous PlatformKeysService
// operation has finished and provides access to the results.
// Note: all PlatformKeysService operations have a trailing const std::string&
// error_message argument that is filled in case of an error.
template <typename... ResultCallbackArgs>
class ExecutionWaiter {
 public:
  ExecutionWaiter() = default;
  ~ExecutionWaiter() = default;
  ExecutionWaiter(const ExecutionWaiter& other) = delete;
  ExecutionWaiter& operator=(const ExecutionWaiter& other) = delete;

  // Returns the callback to be passed to the PlatformKeysService operation
  // invocation.
  base::RepeatingCallback<void(ResultCallbackArgs... result_callback_args,
                               const std::string& error_message)>
  GetCallback() {
    return base::BindRepeating(&ExecutionWaiter::OnExecutionDone,
                               weak_ptr_factory_.GetWeakPtr());
  }

  // Waits until the callback returned by GetCallback() has been called.
  void Wait() { run_loop_.Run(); }

  // Returns the error message passed to the callback.
  const std::string& error_message() {
    EXPECT_TRUE(done_);
    return error_message_;
  }

 protected:
  // A std::tuple that is capable of storing the arguments passed to the result
  // callback.
  using ResultCallbackArgsTuple =
      std::tuple<std::decay_t<ResultCallbackArgs>...>;

  // Access to the arguments passed to the callback.
  const ResultCallbackArgsTuple& result_callback_args() const {
    EXPECT_TRUE(done_);
    return result_callback_args_;
  }

 private:
  void OnExecutionDone(ResultCallbackArgs... result_callback_args,
                       const std::string& error_message) {
    EXPECT_FALSE(done_);
    done_ = true;
    result_callback_args_ = ResultCallbackArgsTuple(
        std::forward<ResultCallbackArgs>(result_callback_args)...);
    error_message_ = error_message;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool done_ = false;
  ResultCallbackArgsTuple result_callback_args_;
  std::string error_message_;

  base::WeakPtrFactory<ExecutionWaiter> weak_ptr_factory_{this};
};

// Supports waiting for the result of PlatformKeysService::GetTokens.
class GetTokensExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<std::vector<std::string>>> {
 public:
  GetTokensExecutionWaiter() = default;
  ~GetTokensExecutionWaiter() = default;

  const std::unique_ptr<std::vector<std::string>>& token_ids() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::GenerateKey*
// function family.
class GenerateKeyExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  GenerateKeyExecutionWaiter() = default;
  ~GenerateKeyExecutionWaiter() = default;

  const std::string& public_key_spki_der() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::Sign* function
// family.
class SignExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  SignExecutionWaiter() = default;
  ~SignExecutionWaiter() = default;

  const std::string& signature() const {
    return std::get<0>(result_callback_args());
  }
};
}  // namespace

class PlatformKeysServiceBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<TestConfig> {
 public:
  PlatformKeysServiceBrowserTest() = default;
  ~PlatformKeysServiceBrowserTest() override = default;
  PlatformKeysServiceBrowserTest(const PlatformKeysServiceBrowserTest& other) =
      delete;
  PlatformKeysServiceBrowserTest& operator=(
      const PlatformKeysServiceBrowserTest& other) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Call |Request*PolicyUpdate| even if not setting affiliation IDs so
    // (empty) policy blobs are prepared in FakeSessionManagerClient.
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();
    auto device_policy_update = device_state_mixin_.RequestDevicePolicyUpdate();
    if (GetParam().profile_to_use == ProfileToUse::kAffiliatedUserProfile) {
      device_policy_update->policy_data()->add_device_affiliation_ids(
          kTestAffiliationId);
      user_policy_update->policy_data()->add_user_affiliation_ids(
          kTestAffiliationId);
    }
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    if (GetParam().profile_to_use == ProfileToUse::kSigninProfile) {
      profile_ = ProfileHelper::GetSigninProfile();
    } else {
      ASSERT_TRUE(login_manager_mixin_.LoginAndWaitForActiveSession(
          LoginManagerMixin::CreateDefaultUserContext(test_user_info_)));
      profile_ = ProfileManager::GetActiveUserProfile();
    }
    ASSERT_TRUE(profile_);

    platform_keys_service_ =
        PlatformKeysServiceFactory::GetForBrowserContext(profile_);
    ASSERT_TRUE(platform_keys_service_);
  }

 protected:
  PlatformKeysService* platform_keys_service() {
    return platform_keys_service_;
  }

 private:
  const AccountId test_user_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));
  const LoginManagerMixin::TestUserInfo test_user_info_{test_user_account_id_};

  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_info_}};

  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_account_id_};

  // Unowned pointer to the profile selected by the current TestConfig.
  // Valid after SetUpOnMainThread().
  Profile* profile_ = nullptr;
  // Unowned pointer to the PlatformKeysService for |profile_|. Valid after
  // SetUpOnMainThread().
  PlatformKeysService* platform_keys_service_ = nullptr;
};

// Tests that GetTokens() is callable and returns the expected tokens.
IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest, GetTokens) {
  GetTokensExecutionWaiter get_tokens_waiter;
  platform_keys_service()->GetTokens(get_tokens_waiter.GetCallback());
  get_tokens_waiter.Wait();

  EXPECT_TRUE(get_tokens_waiter.error_message().empty());
  ASSERT_TRUE(get_tokens_waiter.token_ids());
  EXPECT_THAT(*get_tokens_waiter.token_ids(),
              ::testing::UnorderedElementsAreArray(GetParam().token_ids));
}

// Generates a Rsa key pair and tests signing using that key pair.
IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest, GenerateRsaAndSign) {
  const std::string data_to_sign = "test";
  const unsigned int key_size = 2048;
  const HashAlgorithm hash_algorithm = HASH_ALGORITHM_SHA256;
  const crypto::SignatureVerifier::SignatureAlgorithm signature_algorithm =
      crypto::SignatureVerifier::RSA_PKCS1_SHA256;

  for (const std::string& token_id : GetParam().token_ids) {
    GenerateKeyExecutionWaiter generate_key_waiter;
    platform_keys_service()->GenerateRSAKey(token_id, key_size,
                                            generate_key_waiter.GetCallback());
    generate_key_waiter.Wait();
    EXPECT_TRUE(generate_key_waiter.error_message().empty());

    const std::string public_key_spki_der =
        generate_key_waiter.public_key_spki_der();
    EXPECT_FALSE(public_key_spki_der.empty());

    SignExecutionWaiter sign_waiter;
    platform_keys_service()->SignRSAPKCS1Digest(
        token_id, data_to_sign, public_key_spki_der, hash_algorithm,
        sign_waiter.GetCallback());
    sign_waiter.Wait();
    EXPECT_TRUE(sign_waiter.error_message().empty());

    crypto::SignatureVerifier signature_verifier;
    ASSERT_TRUE(signature_verifier.VerifyInit(
        signature_algorithm,
        base::as_bytes(base::make_span(sign_waiter.signature())),
        base::as_bytes(base::make_span(public_key_spki_der))));
    signature_verifier.VerifyUpdate(
        base::as_bytes(base::make_span(data_to_sign)));
    EXPECT_TRUE(signature_verifier.VerifyFinal());
  }
}

// TODO(https://crbug.com/1067591): Enable for sign-in screen by adding
// ProfileToUse::kSigninProfile.
INSTANTIATE_TEST_SUITE_P(
    AllSupportedProfileTypes,
    PlatformKeysServiceBrowserTest,
    ::testing::Values(
        TestConfig{ProfileToUse::kSigninProfile, {"system"}},
        TestConfig{ProfileToUse::kUnaffiliatedUserProfile, {"user"}},
        TestConfig{ProfileToUse::kAffiliatedUserProfile, {"user", "system"}}));

}  // namespace platform_keys
}  // namespace chromeos
