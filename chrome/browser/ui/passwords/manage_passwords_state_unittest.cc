// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using password_manager::PasswordFormManager;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kTestOrigin[] = "http://example.com/";
constexpr char kTestPSLOrigin[] = "http://1.example.com/";

std::vector<const PasswordForm*> GetRawPointers(
    const std::vector<std::unique_ptr<PasswordForm>>& forms) {
  std::vector<const PasswordForm*> result;
  std::transform(
      forms.begin(), forms.end(), std::back_inserter(result),
      [](const std::unique_ptr<PasswordForm>& form) { return form.get(); });
  return result;
}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD0(UpdateFormManagers, void());
};

class ManagePasswordsStateTest : public testing::Test {
 public:
  ManagePasswordsStateTest() { fetcher_.Fetch(); }

  void SetUp() override {
    saved_match_.origin = GURL(kTestOrigin);
    saved_match_.signon_realm = kTestOrigin;
    saved_match_.username_value = base::ASCIIToUTF16("username");
    saved_match_.username_element = base::ASCIIToUTF16("username_element");
    saved_match_.password_value = base::ASCIIToUTF16("12345");
    saved_match_.password_element = base::ASCIIToUTF16("password_element");

    psl_match_ = saved_match_;
    psl_match_.origin = GURL(kTestPSLOrigin);
    psl_match_.signon_realm = kTestPSLOrigin;
    psl_match_.username_value = base::ASCIIToUTF16("username_psl");
    psl_match_.is_public_suffix_match = true;

    local_federated_form_ = saved_match_;
    local_federated_form_.federation_origin =
        url::Origin::Create(GURL("https://idp.com"));
    local_federated_form_.password_value.clear();
    local_federated_form_.signon_realm =
        "federation://example.com/accounts.com";

    // Create a simple sign-in form.
    observed_form_.url = saved_match_.origin;
    FormFieldData field;
    field.name = ASCIIToUTF16("username_element");
    field.form_control_type = "text";
    observed_form_.fields.push_back(field);
    field.name = ASCIIToUTF16("password_element");
    field.form_control_type = "password";
    observed_form_.fields.push_back(field);

    submitted_form_ = observed_form_;
    const base::string16 new_username = ASCIIToUTF16("new one");
    const base::string16 new_password = ASCIIToUTF16("asdfjkl;");
    // Set username and password.
    submitted_form_.fields[0].value = new_username;
    submitted_form_.fields[1].value = new_password;

    passwords_data_.set_client(&mock_client_);

    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  }

  PasswordForm& saved_match() { return saved_match_; }
  PasswordForm& psl_match() { return psl_match_; }
  FormData& submitted_form() { return submitted_form_; }
  PasswordForm& local_federated_form() { return local_federated_form_; }
  std::vector<const PasswordForm*>& test_stored_forms() {
    return test_stored_forms_;
  }
  FormData& observed_form() { return observed_form_; }
  ManagePasswordsState& passwords_data() { return passwords_data_; }
  password_manager::PasswordManagerDriver& driver() { return driver_; }

  // Returns a PasswordFormManager containing |test_stored_forms_| as the
  // best matches.
  // TODO(https://crbug.com/998496): Create a mock object which implements
  // PasswordFormManagerForUI interface.
  std::unique_ptr<PasswordFormManager> CreateFormManager();

  // Returns a PasswordFormManager containing local_federated_form() as
  // a stored federated credential.
  // TODO(https://crbug.com/998496): Create a mock object which implements
  // PasswordFormManagerForUI interface.
  std::unique_ptr<PasswordFormManager> CreateFormManagerWithFederation();

  // Pushes irrelevant updates to |passwords_data_| and checks that they don't
  // affect the state.
  void TestNoisyUpdates();

  // Pushes both relevant and irrelevant updates to |passwords_data_|.
  void TestAllUpdates();

  // Pushes a blacklisted form and checks that it doesn't affect the state.
  void TestBlacklistedUpdates();

  MOCK_METHOD1(CredentialCallback, void(const PasswordForm*));

 private:
  // Implements both CreateFormManager and CreateFormManagerWithFederation.
  std::unique_ptr<PasswordFormManager> CreateFormManagerInternal(
      bool include_federated);

  MockPasswordManagerClient mock_client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::FakeFormFetcher fetcher_;

  ManagePasswordsState passwords_data_;
  PasswordForm saved_match_;
  PasswordForm psl_match_;
  FormData submitted_form_;
  PasswordForm local_federated_form_;
  FormData observed_form_;
  std::vector<const PasswordForm*> test_stored_forms_;
};

std::unique_ptr<PasswordFormManager>
ManagePasswordsStateTest::CreateFormManager() {
  return CreateFormManagerInternal(false);
}

std::unique_ptr<PasswordFormManager>
ManagePasswordsStateTest::CreateFormManagerWithFederation() {
  return CreateFormManagerInternal(true);
}

std::unique_ptr<PasswordFormManager>
ManagePasswordsStateTest::CreateFormManagerInternal(bool include_federated) {
  auto form_manager = std::make_unique<PasswordFormManager>(
      &mock_client_, driver_.AsWeakPtr(), observed_form(), &fetcher_,
      base::WrapUnique(new password_manager::StubFormSaver),
      nullptr /*  metrics_recorder */);
  fetcher_.SetNonFederated(test_stored_forms_);
  fetcher_.NotifyFetchCompleted();
  if (include_federated)
    fetcher_.set_federated({&local_federated_form()});
  EXPECT_EQ(include_federated ? 1u : 0u,
            form_manager->GetFormFetcher()->GetFederatedMatches().size());
  if (include_federated) {
    EXPECT_EQ(local_federated_form(),
              *form_manager->GetFormFetcher()->GetFederatedMatches().front());
  }
  return form_manager;
}

void ManagePasswordsStateTest::TestNoisyUpdates() {
  const std::vector<const PasswordForm*> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();

  // Push "Add".
  PasswordForm form;
  form.origin = GURL("http://3rdparty.com");
  form.username_value = base::ASCIIToUTF16("username");
  form.password_value = base::ASCIIToUTF16("12345");
  PasswordStoreChange change(PasswordStoreChange::ADD, form);
  PasswordStoreChangeList list(1, change);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Update the form.
  form.password_value = base::ASCIIToUTF16("password");
  list[0] = PasswordStoreChange(PasswordStoreChange::UPDATE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

void ManagePasswordsStateTest::TestAllUpdates() {
  const std::vector<const PasswordForm*> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();
  EXPECT_NE(GURL::EmptyGURL(), origin);

  // Push "Add".
  PasswordForm form;
  GURL::Replacements replace_path;
  replace_path.SetPathStr("absolutely_different_path");
  form.origin = origin.ReplaceComponents(replace_path);
  form.signon_realm = form.origin.GetOrigin().spec();
  form.username_value = base::ASCIIToUTF16("user15");
  form.password_value = base::ASCIIToUTF16("12345");
  PasswordStoreChange change(PasswordStoreChange::ADD, form);
  PasswordStoreChangeList list(1, change);
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  // Remove and Add form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  form.username_value = base::ASCIIToUTF16("user15");
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);
  list.erase(++list.begin());

  // Update the form.
  form.password_value = base::ASCIIToUTF16("password");
  list[0] = PasswordStoreChange(PasswordStoreChange::UPDATE, form);
  EXPECT_CALL(mock_client_, UpdateFormManagers()).Times(0);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  // Delete the form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, form);
  EXPECT_CALL(mock_client_, UpdateFormManagers());
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  Mock::VerifyAndClearExpectations(&mock_client_);

  TestNoisyUpdates();
}

void ManagePasswordsStateTest::TestBlacklistedUpdates() {
  const std::vector<const PasswordForm*> forms =
      GetRawPointers(passwords_data_.GetCurrentForms());
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();
  EXPECT_NE(GURL::EmptyGURL(), origin);

  // Process the blacklisted form.
  PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = origin;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, blacklisted));
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the blacklisted form.
  list[0] = PasswordStoreChange(PasswordStoreChange::REMOVE, blacklisted);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, GetRawPointers(passwords_data().GetCurrentForms()));
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

TEST_F(ManagePasswordsStateTest, DefaultState) {
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());

  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSubmitted) {
  test_stored_forms().push_back(&saved_match());
  test_stored_forms().push_back(&psl_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnPendingPassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSaved) {
  test_stored_forms().push_back(&saved_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE,
            passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSubmittedFederationsPresent) {
  std::unique_ptr<PasswordFormManager> test_form_manager(
      CreateFormManagerWithFederation());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnPendingPassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, OnRequestCredentials) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  const GURL origin = saved_match().origin;
  passwords_data().OnRequestCredentials(std::move(local_credentials), origin);
  passwords_data().set_credentials_callback(base::Bind(
      &ManagePasswordsStateTest::CredentialCallback, base::Unretained(this)));
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();

  EXPECT_CALL(*this, CredentialCallback(nullptr));
  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_TRUE(passwords_data().credentials_callback().is_null());
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  passwords_data().OnAutoSignin(std::move(local_credentials),
                                saved_match().origin);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());
  EXPECT_EQ(saved_match().origin, passwords_data().origin());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(saved_match().origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSave) {
  test_stored_forms().push_back(&psl_match());
  test_stored_forms().push_back(&saved_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());

  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSaveWithFederations) {
  test_stored_forms().push_back(&saved_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(
      CreateFormManagerWithFederation());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);

  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, PasswordAutofilled) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  password_form_map.insert(
      std::make_pair(saved_match().username_value, &saved_match()));
  const GURL origin(kTestOrigin);
  passwords_data().OnPasswordAutofilled(password_form_map, origin, nullptr);

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordAutofillWithSavedFederations) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  password_form_map.insert(
      std::make_pair(saved_match().username_value, &saved_match()));
  const GURL origin(kTestOrigin);
  std::vector<const PasswordForm*> federated;
  federated.push_back(&local_federated_form());
  passwords_data().OnPasswordAutofilled(password_form_map, origin, &federated);

  // |federated| represents the locally saved federations. These are bundled in
  // the "current forms".
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
  // |federated_credentials_forms()| do not refer to the saved federations.
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
}

TEST_F(ManagePasswordsStateTest, PasswordAutofillWithOnlyFederations) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  const GURL origin(kTestOrigin);
  std::vector<const PasswordForm*> federated;
  federated.push_back(&local_federated_form());
  passwords_data().OnPasswordAutofilled(password_form_map, origin, &federated);

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(local_federated_form())));

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
}

TEST_F(ManagePasswordsStateTest, ActiveOnMixedPSLAndNonPSLMatched) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  password_form_map[saved_match().username_value] = &saved_match();
  password_form_map[psl_match().username_value] = &psl_match();
  const GURL origin(kTestOrigin);
  passwords_data().OnPasswordAutofilled(password_form_map, origin, nullptr);

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, InactiveOnPSLMatched) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  password_form_map[psl_match().username_value] = &psl_match();
  passwords_data().OnPasswordAutofilled(password_form_map, GURL(kTestOrigin),
                                        nullptr);

  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());
}

TEST_F(ManagePasswordsStateTest, OnInactive) {
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);

  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  passwords_data().OnInactive();
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());
  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PendingPasswordAddBlacklisted) {
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnPendingPassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, RequestCredentialsAddBlacklisted) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  const GURL origin = saved_match().origin;
  passwords_data().OnRequestCredentials(std::move(local_credentials), origin);
  passwords_data().set_credentials_callback(base::Bind(
      &ManagePasswordsStateTest::CredentialCallback, base::Unretained(this)));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSigninAddBlacklisted) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(saved_match()));
  passwords_data().OnAutoSignin(std::move(local_credentials),
                                saved_match().origin);
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSaveAddBlacklisted) {
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, BackgroundAutofilledAddBlacklisted) {
  std::map<base::string16, const PasswordForm*> password_form_map;
  password_form_map.insert(
      std::make_pair(saved_match().username_value, &saved_match()));
  passwords_data().OnPasswordAutofilled(
      password_form_map, password_form_map.begin()->second->origin, nullptr);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateAddBlacklisted) {
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnUpdatePassword(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateSubmitted) {
  test_stored_forms().push_back(&saved_match());
  test_stored_forms().push_back(&psl_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(saved_match())));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AndroidPasswordUpdateSubmitted) {
  PasswordForm android_form;
  android_form.signon_realm = "android://dHJhc2g=@com.example.android/";
  android_form.origin = GURL(android_form.signon_realm);
  android_form.username_value = submitted_form().fields[0].value;
  android_form.password_value = base::ASCIIToUTF16("old pass");
  test_stored_forms().push_back(&android_form);
  std::unique_ptr<PasswordFormManager> test_form_manager(CreateFormManager());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(android_form)));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());
  EXPECT_EQ(submitted_form().url, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  android_form.password_value = submitted_form().fields[1].value;
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateSubmittedWithFederations) {
  test_stored_forms().push_back(&saved_match());
  std::unique_ptr<PasswordFormManager> test_form_manager(
      CreateFormManagerWithFederation());
  test_form_manager->ProvisionallySave(submitted_form(), &driver(), nullptr);
  passwords_data().OnUpdatePassword(std::move(test_form_manager));

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(saved_match()),
                                   Pointee(local_federated_form())));
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialLocal) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(), saved_match().origin);
  passwords_data().set_credentials_callback(base::Bind(
      &ManagePasswordsStateTest::CredentialCallback, base::Unretained(this)));
  EXPECT_CALL(*this, CredentialCallback(&saved_match()));
  passwords_data().ChooseCredential(&saved_match());
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialEmpty) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(), saved_match().origin);
  passwords_data().set_credentials_callback(base::Bind(
      &ManagePasswordsStateTest::CredentialCallback, base::Unretained(this)));
  EXPECT_CALL(*this, CredentialCallback(nullptr));
  passwords_data().ChooseCredential(nullptr);
}

TEST_F(ManagePasswordsStateTest, ChooseCredentialLocalWithNonEmptyFederation) {
  passwords_data().OnRequestCredentials(
      std::vector<std::unique_ptr<PasswordForm>>(), saved_match().origin);
  passwords_data().set_credentials_callback(base::Bind(
      &ManagePasswordsStateTest::CredentialCallback, base::Unretained(this)));
  EXPECT_CALL(*this, CredentialCallback(&local_federated_form()));
  passwords_data().ChooseCredential(&local_federated_form());
}

}  // namespace
