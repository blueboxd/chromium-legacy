// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"

#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"
#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension_login_screen_mixin.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/pref_names.h"

namespace chromeos {

namespace {

constexpr char kUserEmail[] = "testuser@example.com";

}  // namespace

class ChallengeResponseAuthKeysLoaderBrowserTest : public OobeBaseTest {
 public:
  ChallengeResponseAuthKeysLoaderBrowserTest() {
    // Required for TestCertificateProviderExtensionLoginScreenMixin
    needs_background_networking_ = true;
  }
  ChallengeResponseAuthKeysLoaderBrowserTest(
      const ChallengeResponseAuthKeysLoaderBrowserTest&) = delete;
  ChallengeResponseAuthKeysLoaderBrowserTest& operator=(
      const ChallengeResponseAuthKeysLoaderBrowserTest&) = delete;
  ~ChallengeResponseAuthKeysLoaderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    challenge_response_auth_keys_loader_ =
        std::make_unique<ChallengeResponseAuthKeysLoader>();
    challenge_response_auth_keys_loader_->SetMaxWaitTimeForTesting(
        base::TimeDelta::Max());

    // Register the ChallengeResponseKey for the user.
    user_manager::known_user::SaveKnownUser(account_id_);
  }

  void TearDownOnMainThread() override {
    challenge_response_auth_keys_loader_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  void RegisterChallengeResponseKey(bool with_extension_id) {
    std::vector<ChallengeResponseKey> challenge_response_keys;
    ChallengeResponseKey challenge_response_key;
    challenge_response_key.set_public_key_spki_der(GetSpki());
    if (with_extension_id)
      challenge_response_key.set_extension_id(
          cert_provider_extension_mixin_.GetExtensionId());

    challenge_response_keys.push_back(challenge_response_key);
    base::Value challenge_response_keys_value =
        SerializeChallengeResponseKeysForKnownUser(challenge_response_keys);
    user_manager::known_user::SetChallengeResponseKeys(
        account_id_, std::move(challenge_response_keys_value));
  }

  void OnAvailableKeysLoaded(
      base::RepeatingClosure run_loop_quit_closure,
      std::vector<ChallengeResponseKey>* out_challenge_response_keys,
      std::vector<ChallengeResponseKey> challenge_response_keys) {
    out_challenge_response_keys->swap(challenge_response_keys);
    run_loop_quit_closure.Run();
  }

  ChallengeResponseAuthKeysLoader::LoadAvailableKeysCallback CreateCallback(
      base::RepeatingClosure run_loop_quit_closure,
      std::vector<ChallengeResponseKey>* challenge_response_keys) {
    return base::BindOnce(
        &ChallengeResponseAuthKeysLoaderBrowserTest::OnAvailableKeysLoaded,
        weak_ptr_factory_.GetWeakPtr(), run_loop_quit_closure,
        challenge_response_keys);
  }

  void InstallExtension(bool wait_on_extension_loaded) {
    cert_provider_extension_mixin_.AddExtensionForForceInstallation();
    if (wait_on_extension_loaded) {
      cert_provider_extension_mixin_.WaitUntilExtensionLoaded();
    } else {
      // Even though we do not want to wait until the extension is fully ready,
      // wait until the extension has been registered as a force-installed
      // login-screen extension in profile preferences.
      WaitUntilPrefUpdated();
    }
  }

  void PrefChangedCallback() {
    const PrefService* prefs = ProfileHelper::GetSigninProfile()->GetPrefs();
    const PrefService::Preference* pref =
        prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
    if (pref->IsManaged() && wait_for_pref_change_run_loop_) {
      wait_for_pref_change_run_loop_->Quit();
    }
  }

  std::vector<ChallengeResponseKey> LoadChallengeResponseKeys() {
    base::RunLoop run_loop;
    std::vector<ChallengeResponseKey> challenge_response_keys;
    challenge_response_auth_keys_loader_->LoadAvailableKeys(
        account_id_,
        CreateCallback(run_loop.QuitClosure(), &challenge_response_keys));
    run_loop.Run();
    return challenge_response_keys;
  }

  std::string GetSpki() {
    return cert_provider_extension_mixin_.test_certificate_provider_extension()
        ->GetCertificateSpki();
  }

  std::string GetExtensionId() {
    return cert_provider_extension_mixin_.GetExtensionId();
  }

  AccountId account_id() { return account_id_; }

  ChallengeResponseAuthKeysLoader* challenge_response_auth_keys_loader() {
    return challenge_response_auth_keys_loader_.get();
  }

 private:
  void WaitUntilPrefUpdated() {
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(ProfileHelper::GetSigninProfile()->GetPrefs());
    pref_change_registrar.Add(
        extensions::pref_names::kLoginScreenExtensions,
        base::BindRepeating(
            &ChallengeResponseAuthKeysLoaderBrowserTest::PrefChangedCallback,
            weak_ptr_factory_.GetWeakPtr()));
    const PrefService* prefs = ProfileHelper::GetSigninProfile()->GetPrefs();
    const PrefService::Preference* pref =
        prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
    if (!pref->IsManaged()) {
      base::RunLoop wait_for_pref_change_run_loop;
      wait_for_pref_change_run_loop_ = &wait_for_pref_change_run_loop;
      wait_for_pref_change_run_loop.Run();
    }
  }

  AccountId account_id_{AccountId::FromUserEmail(kUserEmail)};

  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  TestCertificateProviderExtensionLoginScreenMixin
      cert_provider_extension_mixin_{&mixin_host_, &device_state_mixin_,
                                     /*load_extension_immediately=*/false};
  base::RunLoop* wait_for_pref_change_run_loop_ = nullptr;

  std::unique_ptr<ChallengeResponseAuthKeysLoader>
      challenge_response_auth_keys_loader_;

  base::WeakPtrFactory<ChallengeResponseAuthKeysLoaderBrowserTest>
      weak_ptr_factory_{this};
};

// Tests that auth keys can be loaded with an extension providing them already
// in place.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysAfterExtensionIsInstalled) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/true);

  // Check that the extension is registered as a force-installed login-screen
  // extension.
  const PrefService* const prefs =
      ProfileHelper::GetSigninProfile()->GetPrefs();
  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
  EXPECT_TRUE(pref);
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_EQ(pref->GetType(), base::Value::Type::DICTIONARY);
  EXPECT_EQ(pref->GetValue()->DictSize(), size_t{1});

  for (const auto& item : pref->GetValue()->DictItems()) {
    EXPECT_EQ(item.first, GetExtensionId());
  }

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  EXPECT_EQ(challenge_response_keys.size(), size_t{1});
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), GetExtensionId());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

// Tests that auth keys can be loaded while the extension providing them is is
// already registered as force-installed, but installation is not yet complete.
// ChallengeResponseAuthKeysLoader needs to wait on the installation to complete
// instead of incorrectly responding that there are no available certificates.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysWhileExtensionIsBeingInstalled) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);

  // Check that the extension is registered as a force-installed login-screen
  // extension.
  const PrefService* const prefs =
      ProfileHelper::GetSigninProfile()->GetPrefs();
  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
  EXPECT_TRUE(pref);
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_EQ(pref->GetType(), base::Value::Type::DICTIONARY);
  EXPECT_EQ(pref->GetValue()->DictSize(), size_t{1});

  for (const auto& item : pref->GetValue()->DictItems()) {
    EXPECT_EQ(item.first, GetExtensionId());
  }

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  EXPECT_EQ(challenge_response_keys.size(), size_t{1});
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), GetExtensionId());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

// Tests running into the timeout when waiting for extensions.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       TimeoutWhileWaitingOnExtensionInstallation) {
  RegisterChallengeResponseKey(/*with_extension_id=*/true);
  InstallExtension(/*wait_on_extension_loaded=*/false);
  challenge_response_auth_keys_loader()->SetMaxWaitTimeForTesting(
      base::TimeDelta::Min());

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns before any keys are available.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  EXPECT_EQ(challenge_response_keys.size(), size_t{0});
}

// Tests flow when there is no stored extension_id, for backward compatibility.
IN_PROC_BROWSER_TEST_F(ChallengeResponseAuthKeysLoaderBrowserTest,
                       LoadingKeysWithoutExtensionId) {
  RegisterChallengeResponseKey(/*with_extension_id=*/false);
  InstallExtension(/*wait_on_extension_loaded=*/true);

  // Check that the extension is registered as a force-installed login-screen
  // extension.
  const PrefService* const prefs =
      ProfileHelper::GetSigninProfile()->GetPrefs();
  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
  EXPECT_TRUE(pref);
  EXPECT_TRUE(pref->IsManaged());
  EXPECT_EQ(pref->GetType(), base::Value::Type::DICTIONARY);
  EXPECT_EQ(pref->GetValue()->DictSize(), size_t{1});

  for (const auto& item : pref->GetValue()->DictItems()) {
    EXPECT_EQ(item.first, GetExtensionId());
  }

  // Challenge Response Auth Keys can be loaded.
  EXPECT_TRUE(
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id()));

  // LoadAvailableKeys returns the expected keys.
  std::vector<ChallengeResponseKey> challenge_response_keys =
      LoadChallengeResponseKeys();
  EXPECT_EQ(challenge_response_keys.size(), size_t{1});
  EXPECT_EQ(challenge_response_keys.at(0).extension_id(), GetExtensionId());
  EXPECT_EQ(challenge_response_keys.at(0).public_key_spki_der(), GetSpki());
}

}  // namespace chromeos
