// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_password_save_manager.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace password_manager {

namespace {
// Indices of username and password fields in the observed form.
const int kUsernameFieldIndex = 1;
const int kPasswordFieldIndex = 2;
}  // namespace

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(PermanentlyBlacklist, PasswordForm(PasswordStore::FormDigest));
  MOCK_METHOD1(Unblacklist, void(const PasswordStore::FormDigest&));
  MOCK_METHOD3(Save,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD3(Update,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password));
  MOCK_METHOD4(UpdateReplace,
               void(PasswordForm pending,
                    const std::vector<const PasswordForm*>& matches,
                    const base::string16& old_password,
                    const PasswordForm& old_unique_key));
  MOCK_METHOD1(Remove, void(const PasswordForm&));

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetPasswordSyncState, SyncState());

  MOCK_CONST_METHOD0(IsMainFrameSecure, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class MultiStorePasswordSaveManagerTest : public testing::Test {
 public:
  MultiStorePasswordSaveManagerTest()
      : votes_uploader_(&client_,
                        false /* is_possible_change_password_form */) {
    GURL origin = GURL("https://accounts.google.com/a/ServiceLoginAuth");
    GURL action = GURL("https://accounts.google.com/a/ServiceLogin");
    GURL psl_origin = GURL("https://myaccounts.google.com/a/ServiceLoginAuth");
    GURL psl_action = GURL("https://myaccounts.google.com/a/ServiceLogin");

    observed_form_.url = origin;
    observed_form_.action = action;
    observed_form_.name = ASCIIToUTF16("sign-in");
    observed_form_.unique_renderer_id = 1;
    observed_form_.is_form_tag = true;

    FormFieldData field;
    field.name = ASCIIToUTF16("firstname");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 1;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("username");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    observed_form_.fields.push_back(field);

    field.name = ASCIIToUTF16("password");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    observed_form_.fields.push_back(field);

    submitted_form_ = observed_form_;
    submitted_form_.fields[kUsernameFieldIndex].value = ASCIIToUTF16("user1");
    submitted_form_.fields[kPasswordFieldIndex].value = ASCIIToUTF16("secret1");

    saved_match_.origin = origin;
    saved_match_.action = action;
    saved_match_.signon_realm = "https://accounts.google.com/";
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.username_element = ASCIIToUTF16("field1");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.password_element = ASCIIToUTF16("field2");
    saved_match_.is_public_suffix_match = false;
    saved_match_.scheme = PasswordForm::Scheme::kHtml;

    psl_saved_match_ = saved_match_;
    psl_saved_match_.origin = psl_origin;
    psl_saved_match_.action = psl_action;
    psl_saved_match_.signon_realm = "https://myaccounts.google.com/";
    psl_saved_match_.is_public_suffix_match = true;

    parsed_observed_form_ = saved_match_;
    parsed_observed_form_.form_data = observed_form_;
    parsed_observed_form_.username_element =
        observed_form_.fields[kUsernameFieldIndex].name;
    parsed_observed_form_.password_element =
        observed_form_.fields[kPasswordFieldIndex].name;

    parsed_submitted_form_ = parsed_observed_form_;
    parsed_submitted_form_.form_data = submitted_form_;
    parsed_submitted_form_.username_value =
        submitted_form_.fields[kUsernameFieldIndex].value;
    parsed_submitted_form_.password_value =
        submitted_form_.fields[kPasswordFieldIndex].value;

    fetcher_ = std::make_unique<FakeFormFetcher>();
    fetcher_->Fetch();

    metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        client_.IsMainFrameSecure(), client_.GetUkmSourceId());

    auto mock_profile_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_profile_form_saver_ = mock_profile_form_saver.get();

    auto mock_account_form_saver = std::make_unique<NiceMock<MockFormSaver>>();
    mock_account_form_saver_ = mock_account_form_saver.get();

    password_save_manager_ = std::make_unique<MultiStorePasswordSaveManager>(
        std::move(mock_profile_form_saver), std::move(mock_account_form_saver));
    password_save_manager_->Init(&client_, fetcher_.get(), metrics_recorder_,
                                 &votes_uploader_);
  }

  void SetNonFederatedAndNotifyFetchCompleted(
      const std::vector<const autofill::PasswordForm*>& non_federated) {
    fetcher_->SetNonFederated(non_federated);
    fetcher_->NotifyFetchCompleted();
  }

  void SetAccountStoreActive(bool is_active) {
    if (is_active) {
      ON_CALL(*client(), GetPasswordSyncState())
          .WillByDefault(Return(
              password_manager::ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION));
      return;
    }
    ON_CALL(*client(), GetPasswordSyncState())
        .WillByDefault(Return(password_manager::SYNCING_NORMAL_ENCRYPTION));
  }

  void SetDefaultPasswordStore(const autofill::PasswordForm::Store& store) {
    ON_CALL(*client()->GetMockPasswordFeatureManager(),
            GetDefaultPasswordStore())
        .WillByDefault(Return(store));
  }

  MockPasswordManagerClient* client() { return &client_; }
  MockFormSaver* mock_account_form_saver() { return mock_account_form_saver_; }
  MockFormSaver* mock_profile_form_saver() { return mock_profile_form_saver_; }
  FakeFormFetcher* fetcher() { return fetcher_.get(); }
  MultiStorePasswordSaveManager* password_save_manager() {
    return password_save_manager_.get();
  }

  FormData observed_form_;
  FormData submitted_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  PasswordForm parsed_observed_form_;
  PasswordForm parsed_submitted_form_;

 private:
  NiceMock<MockPasswordManagerClient> client_;
  VotesUploader votes_uploader_;
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // Define |fetcher_| before |password_save_manager_|, because the former
  // needs to outlive the latter.
  std::unique_ptr<FakeFormFetcher> fetcher_;
  std::unique_ptr<MultiStorePasswordSaveManager> password_save_manager_;
  NiceMock<MockFormSaver>* mock_account_form_saver_;
  NiceMock<MockFormSaver>* mock_profile_form_saver_;

  DISALLOW_COPY_AND_ASSIGN(MultiStorePasswordSaveManagerTest);
};

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenAccountStoreActive) {
  SetAccountStoreActive(/*is_active=*/true);

  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form(parsed_submitted_form_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(observed_form_, parsed_submitted_form);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       DoNotSaveInAccountStoreWhenAccountStoreInactive) {
  SetAccountStoreActive(/*is_active=*/false);

  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form(parsed_submitted_form_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(observed_form_, parsed_submitted_form);
}

TEST_F(MultiStorePasswordSaveManagerTest, SaveInProfileStore) {
  SetAccountStoreActive(/*is_active=*/true);

  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form(parsed_submitted_form_);
  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(observed_form_, parsed_submitted_form);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreByDefaultIfAccountStoreIsActive) {
  SetAccountStoreActive(/*is_active=*/true);

  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form(parsed_submitted_form_);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(observed_form_, parsed_submitted_form);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInProfileStoreByDefaultIfAccountStoreIsInactive) {
  SetAccountStoreActive(/*is_active=*/false);

  fetcher()->NotifyFetchCompleted();

  PasswordForm parsed_submitted_form(parsed_submitted_form_);

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_TRUE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(observed_form_, parsed_submitted_form);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       UpdateBothStoresIfCredentialsExistInAccountStoreOnly) {
  SetAccountStoreActive(/*is_active=*/true);

  PasswordForm saved_match_in_account_store(saved_match_);
  saved_match_in_account_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Update(_, _, _));

  password_save_manager()->Save(observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       UpdateBothStoresIfCredentialsExistInProfileStoreOnly) {
  SetAccountStoreActive(/*is_active=*/true);

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&saved_match_in_profile_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Update(_, _, _));

  password_save_manager()->Save(observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       UpdateBothStoresIfCredentialsExistInBothStoreOnly) {
  SetAccountStoreActive(/*is_active=*/true);

  PasswordForm saved_match_in_profile_store(saved_match_);
  saved_match_in_profile_store.username_value =
      parsed_submitted_form_.username_value;
  saved_match_in_profile_store.in_store = PasswordForm::Store::kProfileStore;
  PasswordForm saved_match_in_account_store(saved_match_in_profile_store);
  saved_match_in_account_store.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted(
      {&saved_match_in_profile_store, &saved_match_in_account_store});

  password_save_manager()->CreatePendingCredentials(
      parsed_submitted_form_, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_FALSE(password_save_manager()->IsNewLogin());

  EXPECT_CALL(*mock_profile_form_saver(), Update(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Update(_, _, _));

  password_save_manager()->Save(observed_form_, parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       PresaveGeneratedPasswordInAccountStoreIfAccountStoreActive) {
  SetAccountStoreActive(/*is_active=*/true);
  fetcher()->NotifyFetchCompleted();

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       PresaveGeneratedPasswordInProfileStoreIfAccountStoreInactive) {
  SetAccountStoreActive(/*is_active=*/false);
  fetcher()->NotifyFetchCompleted();

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->PresaveGeneratedPassword(parsed_submitted_form_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInAccountStoreWhenPSLMatchExistsInTheAccountStore) {
  SetAccountStoreActive(/*is_active=*/true);
  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.in_store = PasswordForm::Store::kAccountStore;
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      saved_match_, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _)).Times(0);
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _));

  password_save_manager()->Save(observed_form_, saved_match_);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       SaveInProfileStoreWhenPSLMatchExistsInTheProfileStore) {
  SetAccountStoreActive(/*is_active=*/true);
  PasswordForm psl_saved_match(psl_saved_match_);
  psl_saved_match.in_store = PasswordForm::Store::kProfileStore;
  SetNonFederatedAndNotifyFetchCompleted({&psl_saved_match});

  password_save_manager()->CreatePendingCredentials(
      saved_match_, observed_form_, submitted_form_,
      /*is_http_auth=*/false,
      /*is_credential_api_save=*/false);

  EXPECT_CALL(*mock_profile_form_saver(), Save(_, _, _));
  EXPECT_CALL(*mock_account_form_saver(), Save(_, _, _)).Times(0);

  password_save_manager()->Save(observed_form_, saved_match_);
}

TEST_F(MultiStorePasswordSaveManagerTest, UnblacklistInBothStores) {
  SetAccountStoreActive(/*is_active=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);

  EXPECT_CALL(*mock_profile_form_saver(), Unblacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), Unblacklist(form_digest));

  password_save_manager()->Unblacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInAccountStoreWhenAccountStoreActive) {
  SetAccountStoreActive(/*is_active=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest));
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInProfileStoreAlthoughAccountStoreActive) {
  SetAccountStoreActive(/*is_active=*/true);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kProfileStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

TEST_F(MultiStorePasswordSaveManagerTest,
       BlacklistInProfileStoreWhenAccountStoreInactive) {
  SetAccountStoreActive(/*is_active=*/false);
  const PasswordStore::FormDigest form_digest(saved_match_);
  SetDefaultPasswordStore(PasswordForm::Store::kAccountStore);

  EXPECT_CALL(*mock_profile_form_saver(), PermanentlyBlacklist(form_digest));
  EXPECT_CALL(*mock_account_form_saver(), PermanentlyBlacklist(form_digest))
      .Times(0);
  password_save_manager()->PermanentlyBlacklist(form_digest);
}

}  // namespace password_manager
