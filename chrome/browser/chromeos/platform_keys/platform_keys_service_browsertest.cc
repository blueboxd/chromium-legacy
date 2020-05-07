// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/net/nss_context.h"
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
#include "content/public/test/browser_test.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_verifier.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

namespace chromeos {
namespace platform_keys {
namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
constexpr char kSystemToken[] = "system";
constexpr char kUserToken[] = "user";

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

// Softoken NSS PKCS11 module (used for testing) allows only predefined key
// attributes to be set and retrieved. Chaps supports setting and retrieving
// custom attributes.
// This helper is created to allow setting and retrieving attributes
// supported by PlatformKeysService. For the lifetime of an instance of this
// helper, PlatformKeysService will be configured to use softoken key attribute
// mapping for the lifetime of the helper.
class ScopedSoftokenAttrsMapping {
 public:
  explicit ScopedSoftokenAttrsMapping(
      PlatformKeysService* platform_keys_service);
  ScopedSoftokenAttrsMapping(const ScopedSoftokenAttrsMapping& other) = delete;
  ScopedSoftokenAttrsMapping& operator=(
      const ScopedSoftokenAttrsMapping& other) = delete;
  ~ScopedSoftokenAttrsMapping();

 private:
  PlatformKeysService* const platform_keys_service_;
};

ScopedSoftokenAttrsMapping::ScopedSoftokenAttrsMapping(
    PlatformKeysService* platform_keys_service)
    : platform_keys_service_(platform_keys_service) {
  platform_keys_service_->SetMapToSoftokenAttrsForTesting(true);
}

ScopedSoftokenAttrsMapping::~ScopedSoftokenAttrsMapping() {
  DCHECK(platform_keys_service_);
  platform_keys_service_->SetMapToSoftokenAttrsForTesting(false);
}

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

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<net::CertificateList>> {
 public:
  GetCertificatesExecutionWaiter() = default;
  ~GetCertificatesExecutionWaiter() = default;

  const net::CertificateList& matches() const {
    return *std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the
// PlatformKeysService::SetAttributeForKey.
class SetAttributeForKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  SetAttributeForKeyExecutionWaiter() = default;
  ~SetAttributeForKeyExecutionWaiter() = default;
};

// Supports waiting for the result of the
// PlatformKeysService::GetAttributeForKey.
class GetAttributeForKeyExecutionWaiter
    : public ExecutionWaiter<const std::string&> {
 public:
  GetAttributeForKeyExecutionWaiter() = default;
  ~GetAttributeForKeyExecutionWaiter() = default;

  const std::string& attribute_value() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
class RemoveKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  RemoveKeyExecutionWaiter() = default;
  ~RemoveKeyExecutionWaiter() = default;
};

class GetAllKeysExecutionWaiter
    : public ExecutionWaiter<std::vector<std::string>> {
 public:
  GetAllKeysExecutionWaiter() = default;
  ~GetAllKeysExecutionWaiter() = default;

  const std::vector<std::string>& public_keys() const {
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

      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          profile_,
          base::BindRepeating(&PlatformKeysServiceBrowserTest::SetUserSlot,
                              base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }
    ASSERT_TRUE(profile_);

    platform_keys_service_ =
        PlatformKeysServiceFactory::GetForBrowserContext(profile_);
    ASSERT_TRUE(platform_keys_service_);

    scoped_softoken_attrs_mapping_ =
        std::make_unique<ScopedSoftokenAttrsMapping>(platform_keys_service_);
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    // Destroy |scoped_softoken_attrs_mapping_| before |platform_keys_service_|
    // is destroyed.
    scoped_softoken_attrs_mapping_.reset();
  }

 protected:
  PlatformKeysService* platform_keys_service() {
    return platform_keys_service_;
  }

  // Returns the slot to be used depending on |token_id|.
  PK11SlotInfo* GetSlot(const std::string& token_id) {
    if (token_id == kSystemToken) {
      return system_nss_key_slot_mixin_.slot();
    }
    DCHECK_EQ(token_id, kUserToken);
    return user_slot_.get();
  }

  // Generates a key pair in the given |token_id| using platform keys service
  // and returns the SubjectPublicKeyInfo string encoded in DER format.
  std::string GenerateKeyPair(const std::string& token_id) {
    const unsigned int kKeySize = 2048;

    GenerateKeyExecutionWaiter generate_key_waiter;
    platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
                                            generate_key_waiter.GetCallback());
    generate_key_waiter.Wait();

    return generate_key_waiter.public_key_spki_der();
  }

 private:
  void SetUserSlot(const base::Closure& done_callback,
                   net::NSSCertDatabase* db) {
    user_slot_ = db->GetPrivateSlot();
    done_callback.Run();
  }

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
  // The private slot for the profile under test. This should be null if the
  // test parameter mandates testing with the sign-in profile.
  crypto::ScopedPK11Slot user_slot_;
  // Owned pointer to a ScopedSoftokenAttrsMapping object that is created after
  // |platform_keys_service_| is valid.
  std::unique_ptr<ScopedSoftokenAttrsMapping> scoped_softoken_attrs_mapping_;
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
  const unsigned int kKeySize = 2048;
  const HashAlgorithm hash_algorithm = HASH_ALGORITHM_SHA256;
  const crypto::SignatureVerifier::SignatureAlgorithm signature_algorithm =
      crypto::SignatureVerifier::RSA_PKCS1_SHA256;

  for (const std::string& token_id : GetParam().token_ids) {
    GenerateKeyExecutionWaiter generate_key_waiter;
    platform_keys_service()->GenerateRSAKey(token_id, kKeySize,
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

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest, SetAndGetKeyAttribute) {
  // The attribute type to be set and retrieved using platform keys service.
  const KeyAttributeType kAttributeType =
      KeyAttributeType::CertificateProvisioningId;

  for (const std::string& token_id : GetParam().token_ids) {
    const std::string kAttributeValue = "test" + token_id;

    // Generate key pair.
    const std::string public_key_spki_der = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_spki_der.empty());

    // Set key attribute.
    SetAttributeForKeyExecutionWaiter set_attribute_for_key_execution_waiter;
    platform_keys_service()->SetAttributeForKey(
        token_id, public_key_spki_der, kAttributeType, kAttributeValue,
        set_attribute_for_key_execution_waiter.GetCallback());
    set_attribute_for_key_execution_waiter.Wait();

    // Get key attribute.
    GetAttributeForKeyExecutionWaiter get_attribute_for_key_execution_waiter;
    platform_keys_service()->GetAttributeForKey(
        token_id, public_key_spki_der, kAttributeType,
        get_attribute_for_key_execution_waiter.GetCallback());
    get_attribute_for_key_execution_waiter.Wait();

    EXPECT_TRUE(get_attribute_for_key_execution_waiter.error_message().empty());
    EXPECT_EQ(get_attribute_for_key_execution_waiter.attribute_value(),
              kAttributeValue);
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest, GetUnsetKeyAttribute) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::CertificateProvisioningId;

  for (const std::string& token_id : GetParam().token_ids) {
    // Generate key pair.
    const std::string public_key_spki_der = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_spki_der.empty());

    // Get key attribute.
    GetAttributeForKeyExecutionWaiter get_attribute_for_key_execution_waiter;
    platform_keys_service()->GetAttributeForKey(
        token_id, public_key_spki_der, kAttributeType,
        get_attribute_for_key_execution_waiter.GetCallback());
    get_attribute_for_key_execution_waiter.Wait();

    EXPECT_TRUE(get_attribute_for_key_execution_waiter.error_message().empty());
    EXPECT_TRUE(
        get_attribute_for_key_execution_waiter.attribute_value().empty());
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest,
                       GetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::CertificateProvisioningId;
  const std::string kPublicKey = "Non Existing public key";

  for (const std::string& token_id : GetParam().token_ids) {
    // Get key attribute.
    GetAttributeForKeyExecutionWaiter get_attribute_for_key_execution_waiter;
    platform_keys_service()->GetAttributeForKey(
        token_id, kPublicKey, kAttributeType,
        get_attribute_for_key_execution_waiter.GetCallback());
    get_attribute_for_key_execution_waiter.Wait();

    EXPECT_FALSE(
        get_attribute_for_key_execution_waiter.error_message().empty());
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest,
                       SetKeyAttributeForNonExistingKey) {
  const KeyAttributeType kAttributeType =
      KeyAttributeType::CertificateProvisioningId;
  const std::string kAttributeValue = "test";
  const std::string kPublicKey = "Non Existing public key";

  for (const std::string& token_id : GetParam().token_ids) {
    // Set key attribute.
    SetAttributeForKeyExecutionWaiter set_attribute_for_key_execution_waiter;
    platform_keys_service()->SetAttributeForKey(
        token_id, kPublicKey, kAttributeType, kAttributeValue,
        set_attribute_for_key_execution_waiter.GetCallback());
    set_attribute_for_key_execution_waiter.Wait();

    EXPECT_FALSE(
        set_attribute_for_key_execution_waiter.error_message().empty());
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest,
                       RemoveKeyWithNoMatchingCertificates) {
  for (const std::string& token_id : GetParam().token_ids) {
    // Generate first key pair.
    const std::string public_key_1 = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_1.empty());

    // Generate second key pair.
    const std::string public_key_2 = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_2.empty());

    auto public_key_bytes_1 = base::as_bytes(base::make_span(public_key_1));
    auto public_key_bytes_2 = base::as_bytes(base::make_span(public_key_2));
    EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
    EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));

    RemoveKeyExecutionWaiter remove_key_waiter;
    platform_keys_service()->RemoveKey(token_id, public_key_1,
                                       remove_key_waiter.GetCallback());
    remove_key_waiter.Wait();

    EXPECT_TRUE(remove_key_waiter.error_message().empty());
    EXPECT_FALSE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_1));
    EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes_2));
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest,
                       RemoveKeyWithMatchingCertificate) {
  for (const std::string& token_id : GetParam().token_ids) {
    PK11SlotInfo* const slot = GetSlot(token_id);

    // Assert that there are no certificates before importing.
    GetCertificatesExecutionWaiter get_certificates_waiter;
    platform_keys_service()->GetCertificates(
        token_id, get_certificates_waiter.GetCallback());
    get_certificates_waiter.Wait();
    ASSERT_EQ(get_certificates_waiter.matches().size(), 0U);

    // Import testing key pair and certificate.
    net::ScopedCERTCertificate cert;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                          "client_1.pem", "client_1.pk8", slot,
                                          &cert);
    }

    // Assert that the certificate is imported correctly.
    ASSERT_TRUE(cert.get());
    GetCertificatesExecutionWaiter get_certificates_waiter_2;
    platform_keys_service()->GetCertificates(
        token_id, get_certificates_waiter_2.GetCallback());
    get_certificates_waiter_2.Wait();
    ASSERT_EQ(get_certificates_waiter_2.matches().size(), 1U);

    ASSERT_GT(cert->derPublicKey.len, 0U);
    std::string public_key(
        reinterpret_cast<const char*>(cert->derPublicKey.data),
        cert->derPublicKey.len);
    auto public_key_bytes = base::as_bytes(base::make_span(public_key));
    EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes));

    // Try Removing the key pair.
    RemoveKeyExecutionWaiter remove_key_waiter;
    platform_keys_service()->RemoveKey(token_id, public_key,
                                       remove_key_waiter.GetCallback());
    remove_key_waiter.Wait();
    EXPECT_FALSE(remove_key_waiter.error_message().empty());

    // Assert that the certificate is not removed.
    GetCertificatesExecutionWaiter get_certificates_waiter_3;
    platform_keys_service()->GetCertificates(
        token_id, get_certificates_waiter_3.GetCallback());
    get_certificates_waiter_3.Wait();

    net::CertificateList found_certs = get_certificates_waiter_3.matches();
    ASSERT_EQ(found_certs.size(), 1U);
    EXPECT_TRUE(
        net::x509_util::IsSameCertificate(found_certs[0].get(), cert.get()));

    // Assert that the key pair is not deleted.
    EXPECT_TRUE(crypto::FindNSSKeyFromPublicKeyInfo(public_key_bytes));
  }
}

// Generates a key pair in tokens accessible from the profile under test and
// retrieves them.
IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest, GetAllKeys) {
  // Generate key pair in every token.
  std::map<std::string, std::string> token_key_map;
  for (const std::string& token_id : GetParam().token_ids) {
    const std::string public_key_spki_der = GenerateKeyPair(token_id);
    ASSERT_FALSE(public_key_spki_der.empty());
    token_key_map[token_id] = public_key_spki_der;
  }

  // Only keys in the requested token should be retrieved.
  for (const std::string& token_id : GetParam().token_ids) {
    GetAllKeysExecutionWaiter get_all_keys_waiter;
    platform_keys_service()->GetAllKeys(token_id,
                                        get_all_keys_waiter.GetCallback());
    get_all_keys_waiter.Wait();

    EXPECT_TRUE(get_all_keys_waiter.error_message().empty());
    std::vector<std::string> public_keys = get_all_keys_waiter.public_keys();
    ASSERT_EQ(public_keys.size(), 1U);
    EXPECT_EQ(public_keys[0], token_key_map[token_id]);
  }
}

IN_PROC_BROWSER_TEST_P(PlatformKeysServiceBrowserTest,
                       GetAllKeysWhenNoKeysGenerated) {
  for (const std::string& token_id : GetParam().token_ids) {
    GetAllKeysExecutionWaiter get_all_keys_waiter;
    platform_keys_service()->GetAllKeys(token_id,
                                        get_all_keys_waiter.GetCallback());
    get_all_keys_waiter.Wait();

    EXPECT_TRUE(get_all_keys_waiter.error_message().empty());
    std::vector<std::string> public_keys = get_all_keys_waiter.public_keys();
    EXPECT_TRUE(public_keys.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllSupportedProfileTypes,
    PlatformKeysServiceBrowserTest,
    ::testing::Values(TestConfig{ProfileToUse::kSigninProfile, {kSystemToken}},
                      TestConfig{ProfileToUse::kUnaffiliatedUserProfile,
                                 {kUserToken}},
                      TestConfig{ProfileToUse::kAffiliatedUserProfile,
                                 {kSystemToken, kUserToken}}));

}  // namespace platform_keys
}  // namespace chromeos
