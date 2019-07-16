// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/new_password_form_manager.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using autofill::mojom::PasswordFormFieldPredictionType;
using base::ASCIIToUTF16;
using base::Feature;
using base::TestMockTimeTaskRunner;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::IsNull;
using testing::Mock;
using testing::NotNull;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
namespace password_manager {

namespace {

MATCHER_P2(FormUsernamePasswordAre, username, password, "") {
  return arg.username_value == username && arg.password_value == password;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

MATCHER_P(FormIgnoreDate, expected, "") {
  PasswordForm expected_with_date = expected;
  expected_with_date.date_created = arg.date_created;
  return arg == expected_with_date;
}

class MockStoreResultFilter : public StubCredentialsFilter {
 public:
  MOCK_CONST_METHOD1(ShouldSave, bool(const autofill::PasswordForm& form));
  MOCK_CONST_METHOD1(ReportFormLoginSuccess,
                     void(const PasswordFormManagerInterface& form_manager));
  MOCK_CONST_METHOD1(IsSyncAccountEmail, bool(const std::string&));
  MOCK_CONST_METHOD1(ShouldSaveGaiaPasswordHash,
                     bool(const autofill::PasswordForm&));
  MOCK_CONST_METHOD1(ShouldSaveEnterprisePasswordHash,
                     bool(const autofill::PasswordForm&));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() {
    ON_CALL(*this, GetStoreResultFilter()).WillByDefault(Return(&filter_));
    ON_CALL(filter_, ShouldSave(_)).WillByDefault(Return(true));
    ON_CALL(filter_, ShouldSaveGaiaPasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, ShouldSaveEnterprisePasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, IsSyncAccountEmail(_)).WillByDefault(Return(false));
    ON_CALL(*this, IsNewTabPage()).WillByDefault(Return(false));
  }

  MOCK_CONST_METHOD1(IsSavingAndFillingEnabled, bool(const GURL&));
  MOCK_CONST_METHOD0(GetMainFrameCertStatus, net::CertStatus());
  MOCK_METHOD2(AutofillHttpAuth,
               void(const autofill::PasswordForm&,
                    const PasswordFormManagerForUI*));
  MOCK_CONST_METHOD0(GetPasswordStore, PasswordStore*());
  // The code inside EXPECT_CALL for PromptUserToSaveOrUpdatePasswordPtr and
  // ShowManualFallbackForSavingPtr owns the PasswordFormManager* argument.
  MOCK_METHOD1(PromptUserToSaveOrUpdatePasswordPtr,
               void(PasswordFormManagerForUI*));
  MOCK_METHOD3(ShowManualFallbackForSavingPtr,
               void(PasswordFormManagerForUI*, bool, bool));
  MOCK_METHOD0(HideManualFallbackForSaving, void());
  MOCK_METHOD1(NotifySuccessfulLoginWithExistingPassword,
               void(const autofill::PasswordForm&));
  MOCK_METHOD0(AutomaticPasswordSaveIndicator, void());
  MOCK_CONST_METHOD0(GetPrefs, PrefService*());
  MOCK_CONST_METHOD0(GetMainFrameURL, const GURL&());
  MOCK_METHOD0(GetDriver, PasswordManagerDriver*());
  MOCK_CONST_METHOD0(GetStoreResultFilter, const MockStoreResultFilter*());
  MOCK_METHOD0(GetMetricsRecorder, PasswordManagerMetricsRecorder*());
  MOCK_CONST_METHOD0(IsNewTabPage, bool());

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr(manager.release());
    return false;
  }
  void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> manager,
      bool has_generated_password,
      bool is_update) override {
    ShowManualFallbackForSavingPtr(manager.release(), has_generated_password,
                                   is_update);
  }
  void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> manager) override {
    AutomaticPasswordSaveIndicator();
  }

  void FilterAllResultsForSaving() {
    EXPECT_CALL(filter_, ShouldSave(_)).WillRepeatedly(Return(false));
  }

 private:
  testing::NiceMock<MockStoreResultFilter> filter_;
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD1(AutofillDataReceived,
               void(const autofill::FormsPredictionsMap&));
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetPasswordAutofillManager, PasswordAutofillManager*());
};

// Invokes the password store consumer with a single copy of |form|.
ACTION_P(InvokeConsumer, form) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.push_back(std::make_unique<PasswordForm>(form));
  arg0->OnGetPasswordStoreResults(std::move(result));
}

ACTION(InvokeEmptyConsumerWithForms) {
  arg0->OnGetPasswordStoreResults(std::vector<std::unique_ptr<PasswordForm>>());
}

ACTION_P(SaveToScopedPtr, scoped) { scoped->reset(arg0); }

ACTION(DeletePtr) {
  delete arg0;
}

void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.option_values.clear();
    field.option_contents.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id_attribute.clear();
    field.name_attribute.clear();
  }
}

void SetFieldName(const base::string16& name, FormFieldData* field) {
#if defined(OS_IOS)
  field->unique_id = name;
#else
  field->name = name;
#endif
}

// Verifies that |test_ukm_recorder| recorder has a single entry called |entry|
// and returns it.
const ukm::mojom::UkmEntry* GetMetricEntry(
    const ukm::TestUkmRecorder& test_ukm_recorder,
    base::StringPiece entry) {
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      test_ukm_recorder.GetEntriesByName(entry);
  EXPECT_EQ(1u, ukm_entries.size());
  return ukm_entries[0];
}

// Verifies the expectation that |test_ukm_recorder| recorder has a single entry
// called |entry|, and that the entry contains the metric called |metric| set
// to |value|.
template <typename T>
void CheckMetricHasValue(const ukm::TestUkmRecorder& test_ukm_recorder,
                         base::StringPiece entry,
                         base::StringPiece metric,
                         T value) {
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(test_ukm_recorder, entry), metric,
      static_cast<int64_t>(value));
}

// Sets |unique_id| in fields on iOS.
void SetUniqueIdIfNeeded(FormData* form) {
  // On iOS the unique_id member uniquely addresses this field in the DOM.
  // This is an ephemeral value which is not guaranteed to be stable across
  // page loads. It serves to allow a given field to be found during the
  // current navigation.
  // TODO(crbug.com/896689): Expand the logic/application of this to other
  // platforms and/or merge this concept with |unique_renderer_id|.
#if defined(OS_IOS)
  for (auto& f : form->fields)
    f.unique_id = f.id_attribute;
#endif
}

}  // namespace

class PasswordManagerTest : public testing::Test {
 public:
  PasswordManagerTest()
      : test_url_("https://www.example.com"),
        task_runner_(new TestMockTimeTaskRunner) {}
  ~PasswordManagerTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    EXPECT_CALL(*store_, ReportMetrics(_, _, _)).Times(AnyNumber());
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

    ON_CALL(client_, GetPasswordStore()).WillByDefault(Return(store_.get()));
    EXPECT_CALL(*store_, GetSiteStatsImpl(_)).Times(AnyNumber());
    ON_CALL(client_, GetDriver()).WillByDefault(Return(&driver_));

    manager_.reset(new PasswordManager(&client_));
    password_autofill_manager_.reset(
        new PasswordAutofillManager(client_.GetDriver(), nullptr, &client_));

    EXPECT_CALL(driver_, GetPasswordManager())
        .WillRepeatedly(Return(manager_.get()));
    EXPECT_CALL(driver_, GetPasswordAutofillManager())
        .WillRepeatedly(Return(password_autofill_manager_.get()));
    ON_CALL(client_, GetMainFrameCertStatus()).WillByDefault(Return(0));

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));

    ON_CALL(client_, GetMainFrameURL()).WillByDefault(ReturnRef(test_url_));
    ON_CALL(client_, GetMetricsRecorder()).WillByDefault(Return(nullptr));
    ON_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
        .WillByDefault(WithArg<0>(DeletePtr()));
    ON_CALL(client_, ShowManualFallbackForSavingPtr(_, _, _))
        .WillByDefault(WithArg<0>(DeletePtr()));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable waiting, since most tests have nothing to do with predictions.
    // All tests that test working with prediction should explicitly turn
    // predictions on.
    NewPasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  PasswordForm MakeSimpleForm() {
    PasswordForm form;
    form.origin = GURL("http://www.google.com/a/LoginAuth");
    form.action = GURL("http://www.google.com/a/Login");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("googleuser");
    form.password_value = ASCIIToUTF16("p4ssword");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "http://www.google.com/";

    // Fill |form.form_data|.
    form.form_data.url = form.origin;
    form.form_data.action = form.action;
    form.form_data.name = ASCIIToUTF16("the-form-name");
    form.form_data.unique_renderer_id = 10;

    FormFieldData field;
    field.name = ASCIIToUTF16("Email");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.value = ASCIIToUTF16("googleuser");
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    form.form_data.fields.push_back(field);

    field.name = ASCIIToUTF16("Passwd");
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.value = ASCIIToUTF16("p4ssword");
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    form.form_data.fields.push_back(field);

    SetUniqueIdIfNeeded(&form.form_data);
    return form;
  }

  PasswordForm MakeSimpleGAIAForm() {
    PasswordForm form = MakeSimpleForm();
    form.origin = GURL("https://accounts.google.com");
    form.form_data.url = form.origin;
    form.signon_realm = form.origin.spec();
    return form;
  }

  PasswordForm MakeGAIAChangePasswordForm() {
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    form.origin = GURL("https://accounts.google.com");
    form.form_data.url = form.origin;
    form.action = GURL("http://www.google.com/a/Login");
    form.form_data.action = form.action;
    form.form_data.name = ASCIIToUTF16("the-form-name");
    form.signon_realm = form.origin.spec();
    return form;
  }

  // Create a sign-up form that only has a new password field.
  PasswordForm MakeFormWithOnlyNewPasswordField() {
    PasswordForm form = MakeSimpleForm();
    form.new_password_element.swap(form.password_element);
    form.new_password_value.swap(form.password_value);
    form.form_data.fields[1].autocomplete_attribute = "new-password";
    return form;
  }

  PasswordForm MakeAndroidCredential() {
    PasswordForm android_form;
    android_form.origin = GURL("android://hash@google.com");
    android_form.signon_realm = "android://hash@google.com";
    android_form.username_value = ASCIIToUTF16("google");
    android_form.password_value = ASCIIToUTF16("password");
    android_form.is_affiliation_based_match = true;
    return android_form;
  }

  // Reproduction of the form present on twitter's login page.
  PasswordForm MakeTwitterLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("Email");
    form.password_element = ASCIIToUTF16("Passwd");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com/";
    return form;
  }

  // Reproduction of the form present on twitter's failed login page.
  PasswordForm MakeTwitterFailedLoginForm() {
    PasswordForm form;
    form.origin = GURL("https://twitter.com/login/error?redirect_after_login");
    form.action = GURL("https://twitter.com/sessions");
    form.username_element = ASCIIToUTF16("EmailField");
    form.password_element = ASCIIToUTF16("PasswdField");
    form.username_value = ASCIIToUTF16("twitter");
    form.password_value = ASCIIToUTF16("password");
    form.submit_element = ASCIIToUTF16("signIn");
    form.signon_realm = "https://twitter.com/";
    return form;
  }

  PasswordForm MakeSimpleFormWithOnlyPasswordField() {
    PasswordForm form(MakeSimpleForm());
    form.username_element.clear();
    form.username_value.clear();
    // Remove username field in |form_data|.
    form.form_data.fields.erase(form.form_data.fields.begin());
    return form;
  }

  PasswordForm MakeSimpleCreditCardForm() {
    PasswordForm form;
    form.origin = GURL("https://accounts.google.com");
    form.signon_realm = form.origin.spec();
    form.username_element = ASCIIToUTF16("cc-number");
    form.password_element = ASCIIToUTF16("cvc");
    form.username_value = ASCIIToUTF16("1234567");
    form.password_value = ASCIIToUTF16("123");
    form.form_data.url = form.origin;

    FormFieldData field;
    field.name = form.username_element;
    field.id_attribute = field.name;
    field.value = form.username_value;
    field.form_control_type = "text";
    field.unique_renderer_id = 2;
    field.autocomplete_attribute = "cc-name";
    form.form_data.fields.push_back(field);

    field.name = form.password_element;
    field.id_attribute = field.name;
    field.value = form.password_value;
    field.form_control_type = "password";
    field.unique_renderer_id = 3;
    field.autocomplete_attribute = "cc-number";
    form.form_data.fields.push_back(field);

    SetUniqueIdIfNeeded(&form.form_data);

    return form;
  }

  PasswordManager* manager() { return manager_.get(); }

  void OnPasswordFormSubmitted(const PasswordForm& form) {
    manager()->OnPasswordFormSubmitted(&driver_, form);
  }

  void TurnOnNewParsingForFilling(
      base::test::ScopedFeatureList* scoped_feature_list,
      bool enabled) {
    std::vector<Feature> enabled_features;
    std::vector<Feature> disabled_features;
    (enabled ? enabled_features : disabled_features) =
        std::vector<Feature>({features::kNewPasswordFormParsing});

    disabled_features.push_back(features::kNewPasswordFormParsingForSaving);
    disabled_features.push_back(features::kOnlyNewParser);
    scoped_feature_list->InitWithFeatures(enabled_features, disabled_features);
    manager_.reset(new PasswordManager(&client_));
  }

  void TurnOnNewParsingForSaving(
      base::test::ScopedFeatureList* scoped_feature_list,
      bool enabled) {
    std::vector<Feature> enabled_features;
    std::vector<Feature> disabled_features;
    (enabled ? enabled_features : disabled_features) =
        std::vector<Feature>({features::kNewPasswordFormParsing,
                              features::kNewPasswordFormParsingForSaving});
    scoped_feature_list->InitWithFeatures(enabled_features, disabled_features);

    manager_.reset(new PasswordManager(&client_));
  }

  void TurnOnOnlyNewParser(base::test::ScopedFeatureList* scoped_feature_list) {
    scoped_feature_list->InitWithFeatures(
        {features::kNewPasswordFormParsing,
         features::kNewPasswordFormParsingForSaving, features::kOnlyNewParser},
        {});
    manager_.reset(new PasswordManager(&client_));
  }

  const GURL test_url_;
  base::test::ScopedTaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStore> store_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  MockPasswordManagerDriver driver_;
  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;
  std::unique_ptr<PasswordManager> manager_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.origin == arg.origin &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value &&
         form.new_password_element == arg.new_password_element;
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyNewPasswordField) {
  // Test that when a form only contains a "new password" field, the form gets
  // saved and in password store, the new password value is saved as a current
  // password value.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm saved_form;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // The value of the new password field should have been promoted to, and saved
  // to the password store as the current password.
  PasswordForm expected_form(form);
  expected_form.password_value.swap(expected_form.new_password_value);
  expected_form.password_element.swap(expected_form.new_password_element);
  EXPECT_THAT(saved_form, FormMatches(expected_form));
}

TEST_F(PasswordManagerTest, GeneratedPasswordFormSubmitEmptyStore) {
  for (bool new_parsing_for_saving : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "new_parsing_for_saving = " << new_parsing_for_saving);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list, new_parsing_for_saving);
    // Test that generated passwords are stored without asking the user.
    std::vector<PasswordForm> observed;
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    observed.push_back(form);
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    // Simulate the user generating the password and submitting the form.
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*store_, AddLogin(_));
    form.password_value = form.new_password_value;
    manager()->OnPresaveGeneratedPassword(&driver_, form);
    OnPasswordFormSubmitted(form);

    // The user should not need to confirm saving as they have already given
    // consent by using the generated password. The form should be saved once
    // navigation occurs. The client will be informed that automatic saving has
    // occurred.
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    PasswordForm form_to_save;
    EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _))
        .WillOnce(SaveArg<0>(&form_to_save));
    EXPECT_CALL(client_, AutomaticPasswordSaveIndicator());

    // Now the password manager waits for the navigation to complete.
    observed.clear();
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);
    EXPECT_EQ(form.username_value, form_to_save.username_value);
    // What was "new password" field in the submitted form, becomes the current
    // password field in the form to save.
    EXPECT_EQ(form.new_password_value, form_to_save.password_value);
  }
}

#if defined(OS_IOS)
TEST_F(PasswordManagerTest, EditingGeneratedPasswordOnIOS) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form = MakeSimpleForm();
  base::string16 username = form.username_value;
  base::string16 generated_password = form.password_value + ASCIIToUTF16("1");
  const base::string16 username_element = form.username_element;
  const base::string16 generation_element = form.password_element;

  // A form is found by PasswordManager.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, {form});

  // The user is generating the password. The password has to be presaved.
  PasswordForm presaved_form;
  EXPECT_CALL(*store_, AddLogin(FormUsernamePasswordAre(form.username_value,
                                                        generated_password)))
      .WillOnce(SaveArg<0>(&presaved_form));
  manager()->PresaveGeneratedPassword(&driver_, form.form_data,
                                      generated_password, generation_element);
  Mock::VerifyAndClearExpectations(store_.get());

  // Test when the user is changing the generated password, presaved credential
  // is updated.
  generated_password += ASCIIToUTF16("1");
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(
                           FormUsernamePasswordAre(form.username_value,
                                                   generated_password),
                           FormHasUniqueKey(presaved_form)))
      .WillOnce(SaveArg<0>(&presaved_form));

  manager()->UpdateGeneratedPasswordOnUserInput(
      form.form_data.name, generation_element, generated_password);
  Mock::VerifyAndClearExpectations(store_.get());

  // Test when the user is changing the username, presaved credential is
  // updated.
  username += ASCIIToUTF16("1");
  EXPECT_CALL(*store_,
              UpdateLoginWithPrimaryKey(
                  FormUsernamePasswordAre(username, generated_password),
                  FormHasUniqueKey(presaved_form)));

  manager()->UpdateGeneratedPasswordOnUserInput(form.form_data.name,
                                                username_element, username);
}

TEST_F(PasswordManagerTest, SavingGeneratedPasswordOnIOS) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form = MakeSimpleForm();
  const base::string16 username = form.username_value;
  base::string16 generated_password = form.password_value + ASCIIToUTF16("1");
  const base::string16 generation_element = form.password_element;

  // A form is found by PasswordManager.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, {form});

  // The user is generating the password.
  EXPECT_CALL(*store_, AddLogin(_));
  generated_password += ASCIIToUTF16("1");
  manager()->PresaveGeneratedPassword(&driver_, form.form_data,
                                      generated_password, generation_element);

  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _));
  // Test when the user is changing the generated password.
  manager()->UpdateGeneratedPasswordOnUserInput(
      form.form_data.name, generation_element, generated_password);

  // The user is submitting the form.
  form.form_data.fields[0].value = username;
  form.form_data.fields[1].value = generated_password;
  OnPasswordFormSubmitted(form);

  // Test that generated passwords are stored without asking the user.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_,
              UpdateLoginWithPrimaryKey(
                  FormUsernamePasswordAre(username, generated_password), _));
  EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator());

  // Now the password manager waits for the navigation to complete.
  manager()->OnPasswordFormsRendered(&driver_, {}, true);
}

TEST_F(PasswordManagerTest, PasswordNoLongerGeneratedOnIOS) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form = MakeSimpleForm();
  const base::string16 generated_password = form.password_value;
  const base::string16 generation_element = form.password_element;

  // A form is found by PasswordManager.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, {form});

  // The user is generating the password.
  PasswordForm presaved_form;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&presaved_form));
  manager()->PresaveGeneratedPassword(&driver_, form.form_data,
                                      generated_password, generation_element);

  // The user is removing password. Check that it is removed from the store.
  EXPECT_CALL(*store_, RemoveLogin(FormHasUniqueKey(presaved_form)));
  manager()->OnPasswordNoLongerGenerated(&driver_);
}
#endif

TEST_F(PasswordManagerTest, FormSubmitNoGoodMatch) {
  // When the password store already contains credentials for a given form, new
  // credentials get still added, as long as they differ in username from the
  // stored ones.
  PasswordForm existing_different(MakeSimpleForm());
  existing_different.username_value = ASCIIToUTF16("google2");

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(existing_different)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  // We still expect an add, since we didn't have a good match.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

#if !defined(OS_IOS)
TEST_F(PasswordManagerTest, BestMatchFormToManager) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager:
  // 1.Only renderer ids are used for matching.
  // 2.If no matched form manager is found, then new one is created.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  std::vector<PasswordForm> observed;
  // Observe the form that will be submitted.
  PasswordForm form(MakeSimpleForm());

  // This form is different from the on that will be submitted.
  PasswordForm no_match_form(MakeSimpleForm());
  no_match_form.form_data.name = ASCIIToUTF16("another-name");
  no_match_form.action = GURL("http://www.google.com/somethingelse");
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("another-field-name");
  no_match_form.form_data.fields.push_back(field);

  observed.push_back(no_match_form);
  observed.push_back(form);

  // Simulate observing forms after navigation the page.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // The form is modified before being submitted and does not match perfectly.
  // Out of the criteria {name, action, signature}, we keep the signature the
  // same and change the rest.
  PasswordForm changed_form(form);
  changed_form.username_element = ASCIIToUTF16("changed-name");
  changed_form.action = GURL("http://www.google.com/changed-action");
  OnPasswordFormSubmitted(changed_form);
  EXPECT_EQ(CalculateFormSignature(form.form_data),
            CalculateFormSignature(changed_form.form_data));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Verify that PasswordFormManager to be save owns the correct pair of
  // observed and submitted forms.
  // TODO(https://crbug.com/831123): Implement subsequent expectation with
  // PasswordFormManagerInterface and avoid casting to PasswordFormManager*.
  PasswordFormManager* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_EQ(form.action, form_manager->observed_form().action);
  EXPECT_EQ(form.form_data.name, form_manager->observed_form().form_data.name);
  EXPECT_EQ(changed_form.action, form_manager->GetSubmittedForm()->action);
  EXPECT_EQ(changed_form.form_data.name,
            form_manager->GetSubmittedForm()->form_data.name);
}

// As long as the is a PasswordFormManager that matches the origin, we should
// not fail to match a submitted PasswordForm to a PasswordFormManager.
TEST_F(PasswordManagerTest, AnyMatchFormToManager) {
  // This test does not make sense for NewPasswordFormManager, because it has a
  // different behaviour, instead of taken any form manager new one is created.
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  // Observe the form that will be submitted.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);

  // Simulate observing forms after navigation the page.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // The form is modified before being submitted and does not match perfectly.
  // We change all of the criteria: {name, action, signature}.
  PasswordForm changed_form(form);
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("another-field-name");
  changed_form.form_data.fields.push_back(field);
  changed_form.username_element = ASCIIToUTF16("changed-name");
  changed_form.action = GURL("http://www.google.com/changed-action");
  OnPasswordFormSubmitted(changed_form);
  EXPECT_NE(CalculateFormSignature(form.form_data),
            CalculateFormSignature(changed_form.form_data));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Verify that we matched the form to a PasswordFormManager, although with the
  // worst possible match.
  // TODO(https://crbug.com/831123): Implement subsequent expectation with
  // PasswordFormManagerInterface and avoid casting to PasswordFormManager*.
  PasswordFormManager* form_manager =
      static_cast<PasswordFormManager*>(form_manager_to_save.get());
  EXPECT_EQ(form.action, form_manager->observed_form().action);
  EXPECT_EQ(form.form_data.name, form_manager->observed_form().form_data.name);
  EXPECT_EQ(changed_form.action, form_manager->GetSubmittedForm()->action);
  EXPECT_EQ(changed_form.form_data.name,
            form_manager->GetSubmittedForm()->form_data.name);
}
#endif

// Tests that a credential wouldn't be saved if it is already in the store.
TEST_F(PasswordManagerTest, DontSaveAlreadySavedCredential) {
  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The user is typing a credential manually. Till the credential is different
  // from the saved one, the fallback should be available.
  PasswordForm incomplete_match(form);
  incomplete_match.password_value =
      form.password_value.substr(0, form.password_value.length() - 1);
  incomplete_match.form_data.fields[1].value = incomplete_match.password_value;
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, incomplete_match);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(incomplete_match));

  base::UserActionTester user_action_tester;

  // The user completes typing the credential. No fallback should be available,
  // because the credential is already in the store.
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true)).Times(0);
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // The user submits the form. No prompt should pop up. The credential is
  // updated in background.
  OnPasswordFormSubmitted(form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_));
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

// Tests that on Chrome sign-in form credentials are not saved.
TEST_F(PasswordManagerTest, DoNotSaveOnChromeSignInForm) {
  PasswordForm form(MakeSimpleForm());
  form.form_data.is_gaia_with_skip_save_password_form = true;
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillRepeatedly(Return(false));
  // The user is typing a credential. No fallback should be available.
  PasswordForm typed_credentials(form);
  typed_credentials.password_value = ASCIIToUTF16("pw");
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, _, _)).Times(0);
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // The user submits the form. No prompt should pop up.
  OnPasswordFormSubmitted(form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Tests that a UKM metric "Login Passed" is sent when the submitted credentials
// are already in the store and OnPasswordFormsParsed is called multiple times.
TEST_F(PasswordManagerTest,
       SubmissionMetricsIsPassedWhenDontSaveAlreadySavedCredential) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(AnyNumber());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The user submits the form.
  OnPasswordFormSubmitted(form);

  // Another call of OnPasswordFormsParsed happens. In production it happens
  // because of some DOM updates.
  manager()->OnPasswordFormsParsed(&driver_, observed);

  EXPECT_CALL(client_, HideManualFallbackForSaving());
  // The call to manual fallback with |form| equal to already saved should close
  // the fallback, but it should not prevent sending metrics.
  manager()->ShowManualFallbackForSaving(&driver_, form);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_));

  // Simulate successful login. Expect "Login Passed" metric.
  base::UserActionTester user_action_tester;
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

TEST_F(PasswordManagerTest, FormSeenThenLeftPage) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // No message from the renderer that a password was submitted. No
  // expected calls.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, FormSubmit) {
  // Test that a plain form submit results in offering to save passwords.
  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, IsPasswordFieldDetectedOnPage) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnOnlyNewParser(&scoped_feature_list);
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, {form});
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->DropFormManagers();
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
}

TEST_F(PasswordManagerTest, FormSubmitWhenPasswordsCannotBeSaved) {
  // Test that a plain form submit doesn't result in offering to save passwords.
  EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillOnce(Return(false));

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// This test verifies a fix for http://crbug.com/236673
TEST_F(PasswordManagerTest, FormSubmitWithFormOnPreviousPage) {
  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.nytimes.com/");
  first_form.form_data.url = first_form.origin;
  first_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  first_form.form_data.action = first_form.action;
  first_form.signon_realm = "http://www.nytimes.com/";
  PasswordForm second_form(MakeSimpleForm());
  second_form.origin = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.form_data.url = second_form.origin;
  second_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.form_data.action = second_form.action;
  second_form.signon_realm = "https://myaccount.nytimes.com/";

  // Pretend that the form is hidden on the first page.
  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now navigate to a second page.
  manager()->DidNavigateMainFrame(true);

  // This page contains a form with the same markup, but on a different
  // URL.
  observed = {second_form};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Now submit this form
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(second_form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(second_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  // Navigation after form submit, no forms appear.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted and make sure
  // that the saved form matches the second form, not the first.
  EXPECT_CALL(*store_, AddLogin(FormMatches(second_form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, FormSubmitInvisibleLogin) {
  // Tests fix of http://crbug.com/28911: if the login form reappears on the
  // subsequent page, but is invisible, it shouldn't count as a failed login.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  // Expect info bar to appear:
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // The form reappears, but is not visible in the layout:
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, InitiallyInvisibleForm) {
  // Make sure an invisible login form still gets autofilled.
  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the
  // old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, FillPasswordsOnDisabledManager) {
  // Test fix for http://crbug.com/158296: Passwords must be filled even if the
  // password manager is disabled.
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(false));
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
}

TEST_F(PasswordManagerTest, PasswordFormReappearance) {
  // If the password form reappears after submit, PasswordManager should deduce
  // that the login failed and not offer saving.
  std::vector<PasswordForm> observed;
  PasswordForm login_form(MakeSimpleForm());
  observed.push_back(login_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(login_form);

  observed.clear();
  // Simulate form reapperance with different path in url and different renderer
  // ids.
  PasswordForm failed_login_form = login_form;
  failed_login_form.form_data.unique_renderer_id += 1000;
  failed_login_form.form_data.url =
      GURL("https://accounts.google.com/login/error?redirect_after_login");
  observed.push_back(failed_login_form);

  // A PasswordForm appears, and is visible in the layout:
  // No expected calls to the PasswordStore...
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, SyncCredentialsNotSaved) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // User should not be prompted and password should not be saved.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(*store_,
              SaveGaiaPasswordHash(
                  "googleuser", form.password_value,
                  metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));
#endif
  // Prefs are needed for failure logging about sync credentials.
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  client_.FilterAllResultsForSaving();

  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
TEST_F(PasswordManagerTest, HashSavedOnGaiaFormWithSkipSavePassword) {
  for (bool did_stop_loading : {false, true}) {
    for (bool only_new_parser : {false, true}) {
      SCOPED_TRACE(testing::Message("did_stop_loading = ")
                   << did_stop_loading
                   << testing::Message(" only_new_parser = ")
                   << only_new_parser);
      base::test::ScopedFeatureList scoped_feature_list;
      if (only_new_parser) {
        TurnOnOnlyNewParser(&scoped_feature_list);
        EXPECT_CALL(*store_, GetLogins(_, _))
            .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
      } else {
        TurnOnNewParsingForFilling(&scoped_feature_list, true);
      }
      std::vector<PasswordForm> observed;
      PasswordForm form(MakeSimpleGAIAForm());
      // Simulate that this is Gaia form that should be ignored for
      // saving/filling.
      form.form_data.is_gaia_with_skip_save_password_form = true;
      observed.push_back(form);

      EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
          .WillRepeatedly(Return(true));

      manager()->OnPasswordFormsParsed(&driver_, observed);
      manager()->OnPasswordFormsRendered(&driver_, observed, true);

      ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
          .WillByDefault(Return(true));
      ON_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
          .WillByDefault(Return(false));
      ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
          .WillByDefault(Return(true));

      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

      EXPECT_CALL(
          *store_,
          SaveGaiaPasswordHash(
              "googleuser", form.password_value,
              metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));

      OnPasswordFormSubmitted(form);
      observed.clear();
      manager()->OnPasswordFormsRendered(&driver_, observed, did_stop_loading);
      testing::Mock::VerifyAndClearExpectations(&client_);
      testing::Mock::VerifyAndClearExpectations(&store_);
    }
  }
}

TEST_F(PasswordManagerTest,
       HashSavedOnGaiaFormWithSkipSavePasswordAndToNTPNavigation) {
  for (bool only_new_parser : {false, true}) {
    SCOPED_TRACE(testing::Message("only_new_parser = ") << only_new_parser);
    base::test::ScopedFeatureList scoped_feature_list;
    if (only_new_parser) {
      TurnOnOnlyNewParser(&scoped_feature_list);
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    } else {
      TurnOnNewParsingForFilling(&scoped_feature_list, true);
    }

    PasswordForm form(MakeSimpleGAIAForm());
    // Simulate that this is Gaia form that should be ignored for
    // saving/filling.
    form.form_data.is_gaia_with_skip_save_password_form = true;
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
        .WillRepeatedly(Return(true));
    manager()->OnPasswordFormsParsed(&driver_, {form});

    ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
        .WillByDefault(Return(true));
    ON_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
        .WillByDefault(Return(false));
    ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
        .WillByDefault(Return(true));

    EXPECT_CALL(
        *store_,
        SaveGaiaPasswordHash(
            "googleuser", form.password_value,
            metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));

    EXPECT_CALL(client_, IsNewTabPage()).WillRepeatedly(Return(true));
    OnPasswordFormSubmitted(form);
    manager()->DidNavigateMainFrame(false);
    testing::Mock::VerifyAndClearExpectations(&client_);
    testing::Mock::VerifyAndClearExpectations(&store_);
  }
}
#endif

// On a successful login with an updated password,
// CredentialsFilter::ReportFormLoginSuccess and CredentialsFilter::ShouldSave
// should be called. The argument of ShouldSave shold be the submitted form.
TEST_F(PasswordManagerTest, ReportFormLoginSuccessAndShouldSaveCalled) {
  PasswordForm stored_form(MakeSimpleForm());

  std::vector<PasswordForm> observed;
  PasswordForm observed_form = stored_form;
  // Different values of |username_element| needed to ensure that it is the
  // |observed_form| and not the |stored_form| what is passed to ShouldSave.
  observed_form.username_element += ASCIIToUTF16("1");
  SetFieldName(observed_form.username_element,
               &observed_form.form_data.fields[0]);
  observed.push_back(observed_form);
  // Simulate that |form| is already in the store, making this an update.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  OnPasswordFormSubmitted(observed_form);

  // Chrome should recognise the successful login and call
  // ReportFormLoginSuccess.
  EXPECT_CALL(*client_.GetStoreResultFilter(), ReportFormLoginSuccess(_));

  PasswordForm submitted_form = observed_form;
  submitted_form.preferred = true;
  EXPECT_CALL(*client_.GetStoreResultFilter(),
              ShouldSave(FormMatches(submitted_form)));
  EXPECT_CALL(*store_, UpdateLogin(_));
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// When there is a sync password saved, and the user successfully uses the
// stored version of it, PasswordManager should not drop that password.
TEST_F(PasswordManagerTest, SyncCredentialsNotDroppedIfUpToDate) {
  PasswordForm form(MakeSimpleGAIAForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  client_.FilterAllResultsForSaving();

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(*store_,
              SaveGaiaPasswordHash(
                  "googleuser", form.password_value,
                  metrics_util::SyncPasswordHashChange::SAVED_IN_CONTENT_AREA));
#endif
  manager()->OnPasswordFormSubmitted(&driver_, form);

  // Chrome should not remove the sync credential, because it was successfully
  // used as stored, and therefore is up to date.
  EXPECT_CALL(*store_, RemoveLogin(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// While sync credentials are not saved, they are still filled to avoid users
// thinking they lost access to their accounts.
TEST_F(PasswordManagerTest, SyncCredentialsStillFilled) {
  PasswordForm form(MakeSimpleForm());
  // Pretend that the password store contains credentials stored for |form|.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));

  client_.FilterAllResultsForSaving();

  // Load the page.
  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&form_data));
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_EQ(form.password_value, form_data.password_field.value);
}

// On failed login attempts, the retry-form can have action scheme changed from
// HTTP to HTTPS (see http://crbug.com/400769). Check that such retry-form is
// considered equal to the original login form, and the attempt recognised as a
// failure.
TEST_F(PasswordManagerTest,
       SeeingFormActionWithOnlyHttpHttpsChangeIsLoginFailure) {
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);

  PasswordForm first_form(MakeSimpleForm());
  first_form.origin = GURL("http://www.xda-developers.com/");
  first_form.form_data.url = first_form.origin;
  first_form.action = GURL("http://forum.xda-developers.com/login.php");

  // |second_form|'s action differs only with it's scheme i.e. *https://*.
  PasswordForm second_form(first_form);
  second_form.action = GURL("https://forum.xda-developers.com/login.php");
  second_form.form_data.action = second_form.action;

  std::vector<PasswordForm> observed;
  observed.push_back(first_form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(first_form);

  // Simulate loading a page, which contains |second_form| instead of
  // |first_form|.
  observed.clear();
  observed.push_back(second_form);

  // Verify that no prompt to save the password is shown.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest,
       ShouldBlockPasswordForSameOriginButDifferentSchemeTest) {
  constexpr struct {
    const char* old_origin;
    const char* new_origin;
    bool result;
  } kTestData[] = {
      // Same origin and same scheme.
      {"https://example.com/login", "https://example.com/login", false},
      // Same host and same scheme, different port.
      {"https://example.com:443/login", "https://example.com:444/login", false},
      // Same host but different scheme (https to http).
      {"https://example.com/login", "http://example.com/login", true},
      // Same host but different scheme (http to https).
      {"http://example.com/login", "https://example.com/login", false},
      // Different TLD, same schemes.
      {"https://example.com/login", "https://example.org/login", false},
      // Different TLD, different schemes.
      {"https://example.com/login", "http://example.org/login", false},
      // Different subdomains, same schemes.
      {"https://sub1.example.com/login", "https://sub2.example.org/login",
       false},
  };

  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    manager()->main_frame_url_ = GURL(test_case.old_origin);
    GURL origin = GURL(test_case.new_origin);
    EXPECT_EQ(
        test_case.result,
        manager()->ShouldBlockPasswordForSameOriginButDifferentScheme(origin));
  }
}

// Tests whether two submissions to the same origin but different schemes
// result in only saving the first submission, which has a secure scheme.
TEST_F(PasswordManagerTest, AttemptedSavePasswordSameOriginInsecureScheme) {
  PasswordForm secure_form(MakeSimpleForm());
  secure_form.origin = GURL("https://example.com/login");
  secure_form.action = GURL("https://example.com/login");
  secure_form.form_data.url = secure_form.origin;
  secure_form.form_data.action = secure_form.action;
  secure_form.signon_realm = "https://example.com/";

  PasswordForm insecure_form(MakeSimpleForm());
  insecure_form.username_element += ASCIIToUTF16("1");
  insecure_form.username_value = ASCIIToUTF16("compromised_user");
  insecure_form.password_value = ASCIIToUTF16("C0mpr0m1s3d_P4ss");
  insecure_form.origin = GURL("http://example.com/home");
  insecure_form.action = GURL("http://example.com/home");
  insecure_form.form_data.url = insecure_form.origin;
  insecure_form.form_data.action = insecure_form.action;
  insecure_form.signon_realm = "http://example.com/";

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(secure_form.origin))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(client_, GetMainFrameURL())
      .WillRepeatedly(ReturnRef(secure_form.origin));

  // Parse, render and submit the secure form.
  std::vector<PasswordForm> observed = {secure_form};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  OnPasswordFormSubmitted(secure_form);

  // Make sure |PromptUserToSaveOrUpdatePassword| gets called, and the resulting
  // form manager is saved.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  EXPECT_CALL(client_, GetMainFrameURL())
      .WillRepeatedly(ReturnRef(insecure_form.origin));

  // Parse, render and submit the insecure form.
  observed = {insecure_form};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(insecure_form.origin))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  OnPasswordFormSubmitted(insecure_form);

  // Expect no further calls to |PromptUserToSaveOrUpdatePassword| due to
  // insecure origin.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

  // Trigger call to |ProvisionalSavePassword| by rendering a page without
  // forms.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Make sure that the form saved by the user is indeed the secure form.
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(secure_form));
}

// Create a form with both a new and current password element. Let the current
// password value be non-empty and the new password value be empty and submit
// the form. While normally saving the new password is preferred (on change
// password forms, that would be the reasonable choice), if the new password is
// empty, this is likely just a slightly misunderstood form, and Chrome should
// save the non-empty current password field.
TEST_F(PasswordManagerTest, DoNotSaveWithEmptyNewPasswordAndNonemptyPassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  ASSERT_FALSE(form.password_value.empty());
  form.new_password_element = ASCIIToUTF16("new_password_element");
  form.new_password_value.clear();
  observed.push_back(form);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the login to complete successfully.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_EQ(form.password_value,
            PasswordFormManager::PasswordToSave(
                form_manager_to_save->GetPendingCredentials())
                .first);
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyPasswordField) {
  // Test to verify that on submitting the HTML password form without having
  // username input filed shows password save promt and saves the password to
  // store.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  std::vector<PasswordForm> observed;

  // Loads passsword form without username input field.
  PasswordForm form(MakeSimpleFormWithOnlyPasswordField());
  observed.push_back(form);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

// Test that if there are two "similar" forms in different frames, both get
// filled. This means slightly different things depending on whether the
// kNewPasswordFormParsing feature is enabled or not, so it is covered by two
// tests below.

// If kNewPasswordFormParsing is enabled, then "similar" is governed by
// NewPasswordFormManager::DoesManage, which in turn delegates to the unique
// renderer ID of the forms being the same. Note, however, that such ID is only
// unique within one renderer process. If different frames on the page are
// rendered by different processes, two unrelated forms can end up with the same
// ID. The test checks that nevertheless each of them gets assigned its own
// NewPasswordFormManager and filled as expected.
TEST_F(PasswordManagerTest, FillPasswordOnManyFrames_SameId) {
  // Setting task runner is required since NewPasswordFormManager uses
  // PostDelayTask for making filling.
  NewPasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  TestMockTimeTaskRunner::ScopedContext scoped_context_(task_runner_.get());

  // Two unrelated forms...
  FormData form_data;
  form_data.url = GURL("http://www.google.com/a/LoginAuth");
  form_data.action = GURL("http://www.google.com/a/Login");
  form_data.fields.resize(2);
  form_data.fields[0].name = ASCIIToUTF16("Email");
  form_data.fields[0].value = ASCIIToUTF16("googleuser");
  form_data.fields[0].unique_renderer_id = 1;
  form_data.fields[0].form_control_type = "text";
  form_data.fields[1].name = ASCIIToUTF16("Passwd");
  form_data.fields[1].value = ASCIIToUTF16("p4ssword");
  form_data.fields[1].unique_renderer_id = 2;
  form_data.fields[1].form_control_type = "password";
  PasswordForm first_form;
  first_form.form_data = form_data;

  form_data.url = GURL("http://www.example.com/");
  form_data.action = GURL("http://www.example.com/");
  form_data.fields[0].name = ASCIIToUTF16("User");
  form_data.fields[0].value = ASCIIToUTF16("exampleuser");
  form_data.fields[0].unique_renderer_id = 3;
  form_data.fields[1].name = ASCIIToUTF16("Pwd");
  form_data.fields[1].value = ASCIIToUTF16("1234");
  form_data.fields[1].unique_renderer_id = 4;
  PasswordForm second_form;
  second_form.form_data = form_data;

  // Make the forms be "similar".
  first_form.form_data.unique_renderer_id =
      second_form.form_data.unique_renderer_id = 7654;

  // Observe the form in the first frame.
  EXPECT_CALL(*store_,
              GetLogins(PasswordStore::FormDigest(first_form.form_data), _))
      .WillOnce(WithArg<1>(InvokeConsumer(first_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_, {first_form});

  // Observe the form in the second frame.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(*store_,
              GetLogins(PasswordStore::FormDigest(second_form.form_data), _))
      .WillOnce(WithArg<1>(InvokeConsumer(second_form)));
  EXPECT_CALL(driver_b, FillPasswordForm(_));
  manager()->OnPasswordFormsParsed(&driver_b, {second_form});
  task_runner_->FastForwardUntilNoTasksRemain();
}

#if !defined(OS_IOS)
// If kNewPasswordFormParsing is disabled, "similar" is governed by
// PasswordFormManager::DoesManage and is related to actual similarity of the
// forms, including having the same signon realm (and hence origin). Should a
// page have two frames with the same origin and a form, and those two forms be
// similar, then it is important to ensure that the single governing
// PasswordFormManager knows about both PasswordManagerDriver instances and
// instructs them to fill.
// TODO(https://crbug.com/949519): Remove this test when the old parser is gone.
TEST_F(PasswordManagerTest, FillPasswordOnManyFrames_SameForm) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForFilling(&scoped_feature_list, false);
  PasswordForm same_form = MakeSimpleForm();

  // Observe the form in the first frame.
  EXPECT_CALL(driver_, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillOnce(WithArg<1>(InvokeConsumer(same_form)));
  manager()->OnPasswordFormsParsed(&driver_, {same_form});

  // Now the form will be seen the second time, in a different frame. The driver
  // for that frame should be told to fill it, but the store should not be asked
  // for it again.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(driver_b, FillPasswordForm(_));
  EXPECT_CALL(*store_, GetLogins(_, _)).Times(0);
  manager()->OnPasswordFormsParsed(&driver_b, {same_form});
}
#endif

TEST_F(PasswordManagerTest, SameDocumentNavigation) {
  // Test that observing a newly submitted form shows the save password bar on
  // call in page navigation.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, SameDocumentBlacklistedSite) {
  // Test that observing a newly submitted form on blacklisted site does notify
  // the embedder on call in page navigation.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // Simulate that blacklisted form stored in store.
  PasswordForm blacklisted_form(form);
  blacklisted_form.username_value = ASCIIToUTF16("");
  blacklisted_form.blacklisted_by_user = true;
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(blacklisted_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // Prefs are needed for failure logging about blacklisting.
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
  EXPECT_TRUE(form_manager_to_save->IsBlacklisted());
}

#if !defined(OS_IOS)
TEST_F(PasswordManagerTest, SavingSignupForms_NoHTMLMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager:
  // 1.Renderer ids are used for matching.
  // 2.HTML attributes are not used.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (action + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm wrong_action_form(form);
  wrong_action_form.action = GURL("http://www.google.com/other/action");
  observed.push_back(wrong_action_form);

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate either form changing or heuristics choosing other fields
  // after the user has entered their information.
  PasswordForm submitted_form(form);
  submitted_form.new_password_element = ASCIIToUTF16("new_password");
  submitted_form.new_password_value = form.password_value;
  submitted_form.password_element.clear();
  submitted_form.password_value.clear();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(submitted_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&form_to_save));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // PasswordManager observed two forms, and should have associate the saved one
  // with the observed form with a matching action.
  EXPECT_EQ(form.action, form_to_save.action);
  // Password values are always saved as the current password value.
  EXPECT_EQ(submitted_form.new_password_value, form_to_save.password_value);
  EXPECT_EQ(submitted_form.new_password_element, form_to_save.password_element);
}

TEST_F(PasswordManagerTest, SavingSignupForms_NoActionMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager:
  // 1.Only renderer ids are used for matching.
  // 2.HTML attributes are not used.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  // Signup forms don't require HTML attributes match in order to save.
  // Verify that we prefer a better match (HTML attributes + origin vs. origin).
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // Change the submit element so we can track which of the two forms is
  // chosen as a better match.
  PasswordForm wrong_submit_form(form);
  wrong_submit_form.submit_element = ASCIIToUTF16("different_signin");
  wrong_submit_form.new_password_element = ASCIIToUTF16("new_password");
  wrong_submit_form.new_password_value = form.password_value;
  wrong_submit_form.password_element.clear();
  wrong_submit_form.password_value.clear();
  observed.push_back(wrong_submit_form);

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  PasswordForm submitted_form(form);
  submitted_form.action = GURL("http://www.google.com/other/action");

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(submitted_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, AddLogin(_)).WillOnce(SaveArg<0>(&form_to_save));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  // PasswordManager observed two forms, and should have associate the saved one
  // with the observed form with a matching action.
  EXPECT_EQ(form.submit_element, form_to_save.submit_element);

  EXPECT_EQ(submitted_form.password_value, form_to_save.password_value);
  EXPECT_EQ(submitted_form.password_element, form_to_save.password_element);
  EXPECT_EQ(submitted_form.username_value, form_to_save.username_value);
  EXPECT_EQ(submitted_form.username_element, form_to_save.username_element);
  EXPECT_TRUE(form_to_save.new_password_element.empty());
  EXPECT_TRUE(form_to_save.new_password_value.empty());
}

TEST_F(PasswordManagerTest, FormSubmittedChangedWithAutofillResponse) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager, because all
  // parsing is in the browser process and
  // |was_parsed_using_autofill_predictions| is not used anymore.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  // This tests verifies that if the observed forms and provisionally saved
  // differ in the choice of the username, the saving still succeeds, as long as
  // the changed form is marked "parsed using autofill predictions".
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate that based on autofill server username prediction, the username
  // of the form changed from the default candidate("Email") to something else.
  // Set the parsed_using_autofill_predictions bit to true to make sure that
  // choice of username is accepted by PasswordManager, otherwise the the form
  // will be rejected as not equal to the observed one. Note that during
  // initial parsing we don't have autofill server predictions yet, that's why
  // observed form and submitted form may be different.
  form.username_element = ASCIIToUTF16("Username");
  form.was_parsed_using_autofill_predictions = true;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}
#endif

TEST_F(PasswordManagerTest, FormSubmittedUnchangedNotifiesClient) {
  // This tests verifies that if the observed forms and provisionally saved
  // forms are the same, then successful submission notifies the client.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  autofill::PasswordForm updated_form;
  autofill::PasswordForm notified_form;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&updated_form));
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce(SaveArg<0>(&notified_form));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_THAT(form, FormMatches(updated_form));
  EXPECT_THAT(form, FormMatches(notified_form));
}

TEST_F(PasswordManagerTest, SaveFormFetchedAfterSubmit) {
  // Test that a password is offered for saving even if the response from the
  // PasswordStore comes after submit.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);

  // GetLogins calls remain unanswered to emulate that PasswordStore did not
  // fetch a form in time before submission.
  PasswordStoreConsumer* store_consumer = nullptr;
  EXPECT_CALL(*store_, GetLogins(_, _)).WillOnce(SaveArg<1>(&store_consumer));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  OnPasswordFormSubmitted(form);

  // Emulate fetching password form from PasswordStore after submission but
  // before post-navigation load.
  ASSERT_TRUE(store_consumer);
  store_consumer->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Simulate saving the form, as if the info bar was accepted.
  EXPECT_CALL(*store_, AddLogin(FormMatches(form)));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
}

TEST_F(PasswordManagerTest, PasswordGeneration_FailedSubmission) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  form.password_value = form.new_password_value;
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  OnPasswordFormSubmitted(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// If the user edits the generated password, but does not remove it completely,
// it should stay treated as a generated password.
TEST_F(PasswordManagerTest, PasswordGenerationPasswordEdited_FailedSubmission) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  form.password_value = form.new_password_value;
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user editing and submitting a different password. Verify that
  // the edited password is the one that is saved.
  form.password_value = ASCIIToUTF16("different_password");
  OnPasswordFormSubmitted(form);

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Generated password are saved even if it looks like the submit failed (the
// form reappeared). Verify that passwords which are no longer marked as
// generated will not be automatically saved.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved_FailedSubmit) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  form.password_value = form.new_password_value;
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user removing generated password and adding a new one.
  form.password_value = ASCIIToUTF16("different_password");
  EXPECT_CALL(*store_, RemoveLogin(_));
  manager()->OnPasswordNoLongerGenerated(&driver_, form);

  OnPasswordFormSubmitted(form);

  // No infobar or prompt is shown if submission fails.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Verify that passwords which are no longer generated trigger the confirmation
// dialog when submitted.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  form.password_value = form.new_password_value;
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user removing generated password and adding a new one.
  form.password_value = ASCIIToUTF16("different_password");
  EXPECT_CALL(*store_, RemoveLogin(_));
  manager()->OnPasswordNoLongerGenerated(&driver_, form);

  OnPasswordFormSubmitted(form);

  // Verify that a normal prompt is shown instead of the force saving UI.
  std::unique_ptr<PasswordFormManagerForUI> form_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_to_save)));
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator()).Times(0);

  // Simulate a successful submission.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

TEST_F(PasswordManagerTest, PasswordGenerationUsernameChanged) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, AddLogin(_));
  form.password_value = form.new_password_value;
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // Simulate user changing the password and username, without ever completely
  // deleting the password.
  form.username_value = ASCIIToUTF16("new_username");
  form.form_data.fields[0].value = form.username_value;
  // For generated password |password_value| is used for saving instead of parse
  // result.
  form.password_value = ASCIIToUTF16("different_password");
  OnPasswordFormSubmitted(form);

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  PasswordForm form_to_save;
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _))
      .WillOnce(SaveArg<0>(&form_to_save));
  EXPECT_CALL(client_, AutomaticPasswordSaveIndicator());

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_EQ(form.username_value, form_to_save.username_value);
  EXPECT_EQ(form.new_password_value, form_to_save.password_value);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  form.password_value = base::ASCIIToUTF16("password");
  PasswordForm sanitized_form(form);
  SanitizeFormData(&sanitized_form.form_data);

  EXPECT_CALL(*store_, AddLogin(FormMatches(sanitized_form)));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  // The user updates the generated password.
  PasswordForm updated_form(form);
  updated_form.password_value = base::ASCIIToUTF16("password_12345");
  PasswordForm sanitized_updated_form(updated_form);
  SanitizeFormData(&sanitized_updated_form.form_data);
  EXPECT_CALL(*store_,
              UpdateLoginWithPrimaryKey(FormMatches(sanitized_updated_form),
                                        FormHasUniqueKey(sanitized_form)));
  manager()->OnPresaveGeneratedPassword(&driver_, updated_form);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", false, 2);

  // The user removes the generated password.
  EXPECT_CALL(*store_, RemoveLogin(FormHasUniqueKey(sanitized_updated_form)));
  manager()->OnPasswordNoLongerGenerated(&driver_, updated_form);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword_NoFormManager) {
  // Checks that GeneratedFormHasNoFormManager metric is sent if there is no
  // corresponding PasswordFormManager for the given form. It should be uncommon
  // case.
  std::vector<PasswordForm> observed;
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  form.password_value = base::ASCIIToUTF16("password");
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", true, 1);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePasswordAndLogin) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  const bool kFalseTrue[] = {false, true};
  for (bool found_matched_logins_in_store : kFalseTrue) {
    SCOPED_TRACE(testing::Message("found_matched_logins_in_store = ")
                 << found_matched_logins_in_store);
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    std::vector<PasswordForm> observed = {form};
    if (found_matched_logins_in_store) {
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
      EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
    } else {
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    }
    EXPECT_CALL(client_, AutomaticPasswordSaveIndicator())
        .Times(found_matched_logins_in_store ? 0 : 1);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    // The user accepts generated password and makes successful login.
    form.password_value = form.new_password_value;
    PasswordForm presaved_form(form);
    if (found_matched_logins_in_store)
      presaved_form.username_value.clear();
    EXPECT_CALL(*store_, AddLogin(FormMatches(presaved_form)));
    manager()->OnPresaveGeneratedPassword(&driver_, form);
    ::testing::Mock::VerifyAndClearExpectations(store_.get());

    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));
    if (!found_matched_logins_in_store)
      EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(
                               _, FormHasUniqueKey(presaved_form)));
    OnPasswordFormSubmitted(form);
    observed.clear();
    std::unique_ptr<PasswordFormManagerForUI> form_manager;
    if (found_matched_logins_in_store) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
          .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager)));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    }
    manager()->DidNavigateMainFrame(true);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    ::testing::Mock::VerifyAndClearExpectations(store_.get());
    EXPECT_CALL(*store_, IsAbleToSavePasswords()).WillRepeatedly(Return(true));
    if (found_matched_logins_in_store) {
      // Credentials should be updated only when the user explicitly chooses.
      ASSERT_TRUE(form_manager);
      EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(
                               _, FormHasUniqueKey(presaved_form)));
      form_manager->Update(form_manager->GetPendingCredentials());
      ::testing::Mock::VerifyAndClearExpectations(store_.get());
    }
  }
}

TEST_F(PasswordManagerTest, SetGenerationElementAndReasonForForm) {
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _));
  manager()->OnPasswordFormsParsed(&driver_, {form});

  manager()->SetGenerationElementAndReasonForForm(&driver_, form,
                                                  ASCIIToUTF16("psw"), false);
  EXPECT_CALL(*store_, AddLogin(_));
  manager()->OnPresaveGeneratedPassword(&driver_, form);

  const PasswordFormManagerInterface* form_manager =
      manager()->form_managers().front().get();

  EXPECT_TRUE(form_manager->HasGeneratedPassword());
}

#if !defined(OS_IOS)
TEST_F(PasswordManagerTest,
       PasswordGenerationNoCorrespondingPasswordFormManager) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager: there is much
  // more robust mechanism for matching them, so no new NewPasswordFormManager
  // is created when no matched one is found.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(0);
  // Verifies that if there is no corresponding password form manager for the
  // given form, new password form manager should fetch data from the password
  // store. Also verifies that |SetGenerationElementAndReasonForForm| doesn't
  // change |has_generated_password_| of new password form manager.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  std::vector<PasswordForm> observed;
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(*store_, GetLogins(PasswordStore::FormDigest(form), _));
  manager()->SetGenerationElementAndReasonForForm(&driver_, form,
                                                  base::string16(), false);
  ASSERT_EQ(1u, manager()->pending_login_managers().size());
  PasswordFormManager* form_manager =
      manager()->pending_login_managers().front().get();

  EXPECT_FALSE(form_manager->HasGeneratedPassword());
}
#endif

TEST_F(PasswordManagerTest, UpdateFormManagers) {
    // Seeing a form should result in creating PasswordFormManager and
    // NewPasswordFormManager and querying PasswordStore. Calling
    // UpdateFormManagers should result in querying the store again.
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

    manager()->OnPasswordFormsParsed(&driver_, {PasswordForm()});

    EXPECT_CALL(*store_, GetLogins(_, _));
    manager()->UpdateFormManagers();
}

#if !defined(OS_IOS)
TEST_F(PasswordManagerTest, DropFormManagers) {
  // This test doesn't make sense for the new parser, because
  // NewPasswordFormManager is created on submission if it is missing.
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  // Interrupt the normal submit flow by DropFormManagers().
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  manager()->DropFormManagers();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form);

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

TEST_F(PasswordManagerTest, AutofillingOfAffiliatedCredentials) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<PasswordForm> observed_forms;
  observed_forms.push_back(observed_form);

  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&form_data));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(android_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  observed_forms.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  EXPECT_EQ(android_form.username_value, form_data.username_field.value);
  EXPECT_EQ(android_form.password_value, form_data.password_field.value);
  EXPECT_FALSE(form_data.wait_for_username);
  EXPECT_EQ(android_form.signon_realm, form_data.preferred_realm);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.origin))
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.form_data.fields[0].value = filled_form.username_value;
  filled_form.password_value = android_form.password_value;
  filled_form.form_data.fields[1].value = filled_form.password_value;
  OnPasswordFormSubmitted(filled_form);

  PasswordForm saved_form;
  PasswordForm saved_notified_form;
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce(SaveArg<0>(&saved_notified_form));
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);

  observed_forms.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);
  EXPECT_THAT(saved_form, FormMatches(android_form));
  EXPECT_THAT(saved_form, FormMatches(saved_notified_form));
}

// If the manager fills a credential originally saved from an affiliated Android
// application, and the user overwrites the password, they should be prompted if
// they want to update.  If so, the Android credential itself should be updated.
TEST_F(PasswordManagerTest, UpdatePasswordOfAffiliatedCredential) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<PasswordForm> observed_forms = {observed_form};

  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(android_form)));
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.origin))
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.form_data.fields[0].value = filled_form.username_value;
  filled_form.password_value = ASCIIToUTF16("new_password");
  filled_form.form_data.fields[1].value = filled_form.password_value;
  OnPasswordFormSubmitted(filled_form);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  observed_forms.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms, true);

  PasswordForm saved_form;
  EXPECT_CALL(*store_, AddLogin(_)).Times(0);
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _)).Times(0);
  EXPECT_CALL(*store_, UpdateLogin(_)).WillOnce(SaveArg<0>(&saved_form));
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();

  PasswordForm expected_form(android_form);
  expected_form.password_value = filled_form.password_value;
  EXPECT_THAT(saved_form, FormMatches(expected_form));
}

TEST_F(PasswordManagerTest, ClearedFieldsSuccessCriteria) {
  // Test that a submission is considered to be successful on a change password
  // form without username when fields valued are cleared.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  form.username_element.clear();
  form.username_value.clear();
  form.form_data.fields[0].value.clear();
  std::vector<PasswordForm> observed = {form};

  // Emulate page load.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  OnPasswordFormSubmitted(form);

  // JavaScript cleared field values.
  observed[0].password_value.clear();
  observed[0].new_password_value.clear();
  observed[0].form_data.fields[1].value.clear();

  // Check success of the submission.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Check that no sync password hash is saved when no username is available,
// because we it's not clear whether the submitted credentials are sync
// credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NoUsername) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  // Simulate that no username is found.
  form.username_value.clear();
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  // Simulate that this credentials which is similar to be sync credentials.
  client_.FilterAllResultsForSaving();

  // Check that no Gaia credential password hash is saved.
  EXPECT_CALL(*store_, SaveGaiaPasswordHash(_, _, _)).Times(0);
  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Check that no sync password hash is saved when the submitted credentials are
// not qualified as sync credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NotSyncCredentials) {
  // Simulate loading a simple form with no existing stored password.
  PasswordForm form(MakeSimpleGAIAForm());
  std::vector<PasswordForm> observed = {form};
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  // Check that no Gaia credential password hash is saved since these
  // credentials are eligible for saving.
  EXPECT_CALL(*store_, SaveGaiaPasswordHash(_, _, _)).Times(0);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  OnPasswordFormSubmitted(form);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

TEST_F(PasswordManagerTest, ManualFallbackForSaving) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm stored_form = form;
  stored_form.password_value = ASCIIToUTF16("old_password");
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = ASCIIToUTF16("another_username");
  new_form.form_data.fields[0].value = new_form.username_value;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, new_form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();

  // Two PasswordFormManagers instances hold references to a shared
  // PasswordFormMetrics recorder. These need to be freed to flush the metrics
  // into the test_ukm_recorder.
  manager_.reset();
  form_manager_to_save.reset();

  // Verify that the last state is recorded.
  CheckMetricHasValue(
      test_ukm_recorder, ukm::builders::PasswordForm::kEntryName,
      ukm::builders::PasswordForm::kSaving_ShowedManualFallbackForSavingName,
      1);
}

// Tests that the manual fallback for saving isn't shown if there is no response
// from the password storage. When crbug.com/741537 is fixed, change this test.
TEST_F(PasswordManagerTest, ManualFallbackForSaving_SlowBackend) {
  for (bool only_new_parser_enabled : {false, true}) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (only_new_parser_enabled)
      TurnOnOnlyNewParser(&scoped_feature_list);
    std::vector<PasswordForm> observed;
    PasswordForm form(MakeSimpleForm());
    observed.push_back(form);
    PasswordStoreConsumer* store_consumer = nullptr;
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
        .WillRepeatedly(Return(true));
    // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
    // the old parser is gone.
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(SaveArg<1>(&store_consumer));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    // There is no response from the store. Don't show the fallback.
    EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, _, _)).Times(0);
    manager()->ShowManualFallbackForSaving(&driver_, form);

    // The storage responded. The fallback can be shown.
    ASSERT_TRUE(store_consumer);
    store_consumer->OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<PasswordForm>>());
    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    manager()->ShowManualFallbackForSaving(&driver_, form);
    Mock::VerifyAndClearExpectations(&client_);
    Mock::VerifyAndClearExpectations(&store_);
  }
}

TEST_F(PasswordManagerTest, ManualFallbackForSaving_GeneratedPassword) {
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // A user accepts a password generated by Chrome. It triggers password
  // presaving and showing manual fallback.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(*store_, AddLogin(_));
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, true, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->OnPresaveGeneratedPassword(&driver_, form);
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // A user edits the generated password. And again it causes password presaving
  // and showing manual fallback.
  EXPECT_CALL(*store_, UpdateLoginWithPrimaryKey(_, _));
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, true, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->OnPresaveGeneratedPassword(&driver_, form);
  manager()->ShowManualFallbackForSaving(&driver_, form);

  // A user removes the generated password. The presaved password is removed,
  // the fallback is disabled.
  EXPECT_CALL(*store_, RemoveLogin(_));
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->OnPasswordNoLongerGenerated(&driver_, form);
  manager()->HideManualFallbackForSaving();
}

// Tests that Autofill predictions are processed correctly. If at least one of
// these predictions can be converted to a |PasswordFormFieldPredictionMap|, the
// predictions map is updated accordingly.
TEST_F(PasswordManagerTest, ProcessAutofillPredictions) {
  // Create FormData form with two fields.
  autofill::FormData form;
  form.url = GURL("http://foo.com");
  autofill::FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  autofill::AutofillQueryResponseContents response;
  // If there are multiple predictions for the field,
  // |AutofillField::overall_server_type_| will store only autofill vote, but
  // not password vote. |AutofillField::server_predictions_| should store all
  // predictions.
  autofill::AutofillQueryResponseContents_Field* field0 = response.add_field();
  field0->set_overall_type_prediction(autofill::PHONE_HOME_NUMBER);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction0 = field0->add_predictions();
  field_prediction0->set_type(autofill::PHONE_HOME_NUMBER);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction1 = field0->add_predictions();
  field_prediction1->set_type(autofill::USERNAME);

  autofill::AutofillQueryResponseContents_Field* field1 = response.add_field();
  field1->set_overall_type_prediction(autofill::PASSWORD);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction2 = field1->add_predictions();
  field_prediction2->set_type(autofill::PASSWORD);
  autofill::AutofillQueryResponseContents_Field_FieldPrediction*
      field_prediction3 = field1->add_predictions();
  field_prediction3->set_type(autofill::PROBABLY_NEW_PASSWORD);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  // Check that Autofill predictions are converted to password related
  // predictions.
  autofill::FormsPredictionsMap predictions;
  predictions[form][form.fields[0]] =
      PasswordFormFieldPredictionType::kUsername;
  predictions[form][form.fields[1]] =
      PasswordFormFieldPredictionType::kCurrentPassword;
  EXPECT_CALL(driver_, AutofillDataReceived(predictions));

  manager()->ProcessAutofillPredictions(&driver_, forms);
}

// Sync password hash should be updated upon submission of change password page.
TEST_F(PasswordManagerTest, SaveSyncPasswordHashOnChangePasswordPage) {
  PasswordForm form(MakeGAIAChangePasswordForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      *store_,
      SaveGaiaPasswordHash(
          "googleuser", form.new_password_value,
          metrics_util::SyncPasswordHashChange::CHANGED_IN_CONTENT_AREA));
#endif
  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Non-Sync Gaia password hash should be saved upon submission of Gaia login
// page.
TEST_F(PasswordManagerTest, SaveOtherGaiaPasswordHash) {
  PasswordForm form(MakeSimpleGAIAForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      *store_,
      SaveGaiaPasswordHash(
          "googleuser", form.password_value,
          metrics_util::SyncPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE));

  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}

// Enterprise password hash should be saved upon submission of enterprise login
// page.
TEST_F(PasswordManagerTest, SaveEnterprisePasswordHash) {
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(client_, GetPrefs()).WillRepeatedly(Return(nullptr));
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveEnterprisePasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(false));
  EXPECT_CALL(*store_,
              SaveEnterprisePasswordHash("googleuser", form.password_value));
  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed, true);
}
#endif

// If there are no forms to parse, certificate errors should not be reported.
TEST_F(PasswordManagerTest, CertErrorReported_NoForms) {
  const std::vector<PasswordForm> observed;
  EXPECT_CALL(client_, GetMainFrameCertStatus())
      .WillRepeatedly(Return(net::CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  base::HistogramTester histogram_tester;
  manager()->OnPasswordFormsParsed(&driver_, observed);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.CertificateErrorsWhileSeeingForms", 0);
}

TEST_F(PasswordManagerTest, CertErrorReported) {
  constexpr struct {
    net::CertStatus cert_status;
    metrics_util::CertificateError expected_error;
  } kCases[] = {
      {0, metrics_util::CertificateError::NONE},
      {net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,  // not an error
       metrics_util::CertificateError::NONE},
      {net::CERT_STATUS_COMMON_NAME_INVALID,
       metrics_util::CertificateError::COMMON_NAME_INVALID},
      {net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
       metrics_util::CertificateError::WEAK_SIGNATURE_ALGORITHM},
      {net::CERT_STATUS_DATE_INVALID,
       metrics_util::CertificateError::DATE_INVALID},
      {net::CERT_STATUS_AUTHORITY_INVALID,
       metrics_util::CertificateError::AUTHORITY_INVALID},
      {net::CERT_STATUS_WEAK_KEY, metrics_util::CertificateError::OTHER},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_WEAK_KEY,
       metrics_util::CertificateError::DATE_INVALID},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_AUTHORITY_INVALID,
       metrics_util::CertificateError::AUTHORITY_INVALID},
      {net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_AUTHORITY_INVALID |
           net::CERT_STATUS_WEAK_KEY,
       metrics_util::CertificateError::AUTHORITY_INVALID},
  };

  const std::vector<PasswordForm> observed = {PasswordForm()};
  EXPECT_CALL(*store_, GetLogins(_, _));

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(testing::Message("index of test_case = ")
                 << (&test_case - kCases));
    EXPECT_CALL(client_, GetMainFrameCertStatus())
        .WillRepeatedly(Return(test_case.cert_status));
    base::HistogramTester histogram_tester;
    manager()->OnPasswordFormsParsed(&driver_, observed);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.CertificateErrorsWhileSeeingForms",
        test_case.expected_error, 1);
  }
}

TEST_F(PasswordManagerTest, CreatingFormManagers) {
  // Add the NewPasswordFormParsing feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewPasswordFormParsing);

  PasswordForm form(MakeSimpleForm());
  std::vector<PasswordForm> observed;
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // Check that the form manager is created.
  EXPECT_EQ(1u, manager()->form_managers().size());
  EXPECT_TRUE(manager()->form_managers()[0]->DoesManage(form.form_data,
                                                        client_.GetDriver()));

  // Check that receiving the same form the second time does not lead to
  // creating new form manager.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_EQ(1u, manager()->form_managers().size());
}

#if !defined(OS_IOS)
TEST_F(PasswordManagerTest,
       ShowManualFallbacksDontChangeProvisionalSaveManager) {
  base::test::ScopedFeatureList scoped_feature_list;
  // This test does not make sense for NewPasswordFormManager because
  // provisional_save_manager() is OldPasswordFormManager.
  TurnOnNewParsingForSaving(&scoped_feature_list, false);
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_THAT(manager()->provisional_save_manager(), IsNull());
  manager()->ShowManualFallbackForSaving(&driver_, form);
  EXPECT_THAT(manager()->provisional_save_manager(), IsNull());

  // The user submits the form and a provisional save manager is set.
  OnPasswordFormSubmitted(form);

  EXPECT_THAT(manager()->provisional_save_manager(), NotNull());
  const PasswordFormManager* last_provisional_save_manager =
      manager()->provisional_save_manager();

  EXPECT_CALL(client_, HideManualFallbackForSaving());
  // The call to manual fallback with |form| equal to already saved should close
  // the fallback.
  manager()->ShowManualFallbackForSaving(&driver_, form);

  EXPECT_THAT(manager()->provisional_save_manager(), NotNull());
  EXPECT_EQ(last_provisional_save_manager,
            manager()->provisional_save_manager());
}
#endif

// Tests that processing normal HTML form submissions works properly with the
// new parsing. For details see scheme 1 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingNormalFormSubmission) {
  for (bool only_new_parser : {false, true}) {
    for (bool successful_submission : {false, true}) {
      SCOPED_TRACE(testing::Message("only_new_parser = ")
                   << only_new_parser
                   << "  successful_submission = " << successful_submission);
      base::test::ScopedFeatureList scoped_feature_list;
      if (only_new_parser)
        TurnOnOnlyNewParser(&scoped_feature_list);
      else
        TurnOnNewParsingForSaving(&scoped_feature_list, true);

      EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
          .WillRepeatedly(Return(true));

      PasswordForm form(MakeSimpleForm());
      EXPECT_CALL(*store_, GetLogins(_, _))
          .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

      std::vector<PasswordForm> observed;
      observed.push_back(form);
      manager()->OnPasswordFormsParsed(&driver_, observed);
      manager()->OnPasswordFormsRendered(&driver_, observed, true);

      if (only_new_parser)
        EXPECT_TRUE(manager()->pending_login_managers().empty());

      auto submitted_form = form;
      submitted_form.form_data.fields[0].value = ASCIIToUTF16("username");
      submitted_form.form_data.fields[1].value = ASCIIToUTF16("password1");

      OnPasswordFormSubmitted(submitted_form);
      EXPECT_TRUE(manager()->GetSubmittedManagerForTest());

      std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;

      // Simulate submission.
      if (successful_submission) {
        EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
            .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
        // The form disappeared, so the submission is condered to be successful.
        observed.clear();
      } else {
        EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
      }
      manager()->OnPasswordFormsRendered(&driver_, observed, true);

      // Multiple calls of OnPasswordFormsRendered should be handled gracefully.
      manager()->OnPasswordFormsRendered(&driver_, observed, true);
      testing::Mock::VerifyAndClearExpectations(&client_);
    }
  }
}

// Tests that processing form submissions without navigations works properly
// with the new parsing. For details see scheme 2 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingOtherSubmissionTypes) {
  for (bool only_new_parser : {false, true}) {
    SCOPED_TRACE(testing::Message("only_new_parser = ") << only_new_parser);
    base::test::ScopedFeatureList scoped_feature_list;
    if (only_new_parser)
      TurnOnOnlyNewParser(&scoped_feature_list);
    else
      TurnOnNewParsingForSaving(&scoped_feature_list, true);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(true));

    PasswordForm form(MakeSimpleForm());
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

    std::vector<PasswordForm> observed;
    observed.push_back(form);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    auto submitted_form = form;
    submitted_form.form_data.fields[0].value = ASCIIToUTF16("username");
    submitted_form.form_data.fields[1].value = ASCIIToUTF16("strong_password");

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    manager()->OnPasswordFormSubmittedNoChecks(&driver_, submitted_form);
    EXPECT_TRUE(manager()->form_managers().empty());
    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

TEST_F(PasswordManagerTest, SubmittedGaiaFormWithoutVisiblePasswordField) {
  // Tests that a submitted GAIA sign-in form which does not contain a visible
  // password field is skipped.
  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleGAIAForm());
  observed.push_back(form);
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));

  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");
  form.form_data.fields[1].is_focusable = false;

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormSubmittedNoChecks(&driver_, form);
}

#if !defined(OS_IOS)
// Tests that PasswordFormManager and NewPasswordFormManager for the same form
// have the same metrics recorder.
TEST_F(PasswordManagerTest, CheckMetricsRecorder) {
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForFilling(&scoped_feature_list, true);

  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed;
  observed.push_back(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);

  const std::vector<std::unique_ptr<PasswordFormManager>>&
      password_form_managers = manager()->pending_login_managers();

  const std::vector<std::unique_ptr<NewPasswordFormManager>>&
      new_password_form_managers = manager()->form_managers();

  ASSERT_EQ(1u, password_form_managers.size());
  ASSERT_EQ(1u, new_password_form_managers.size());

  EXPECT_TRUE(password_form_managers[0]->GetMetricsRecorder());
  EXPECT_EQ(password_form_managers[0]->GetMetricsRecorder(),
            new_password_form_managers[0]->GetMetricsRecorder());
}
#endif

TEST_F(PasswordManagerTest, MetricForSchemeOfSuccessfulLogins) {
  for (bool origin_is_secure : {false, true}) {
    SCOPED_TRACE(testing::Message("origin_is_secure = ") << origin_is_secure);
    PasswordForm form(MakeSimpleForm());
    form.origin =
        GURL(origin_is_secure ? "https://example.com" : "http://example.com");
    form.form_data.url = form.origin;
    std::vector<PasswordForm> observed = {form};
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
        .WillRepeatedly(Return(true));
    OnPasswordFormSubmitted(form);

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

    observed.clear();
    base::HistogramTester histogram_tester;
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed, true);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.SuccessfulLoginHappened", origin_is_secure, 1);
  }
}

TEST_F(PasswordManagerTest, ManualFallbackForSavingNewParser) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);
  NewPasswordFormManager::set_wait_for_server_predictions_for_filling(false);

  std::vector<PasswordForm> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form);
  PasswordForm stored_form = form;
  stored_form.password_value = ASCIIToUTF16("old_password");
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(stored_form)));
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed, true);

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, true))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = ASCIIToUTF16("another_username");
  new_form.form_data.fields[0].value = new_form.username_value;
  EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
  manager()->ShowManualFallbackForSaving(&driver_, new_form);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();
}

#if !defined(OS_IOS)
// Check that some value for the ParsingOnSavingDifference UKM metric is emitted
// on a successful login.
TEST_F(PasswordManagerTest, ParsingOnSavingMetricRecorded) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<Feature> enabled_features;
  std::vector<Feature> disabled_features = {features::kOnlyNewParser};
  // This test does not make sense the old parser is off, since it checks
  // metrics for comparison the old and the new one.
  scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);
  manager_.reset(new PasswordManager(&client_));

  PasswordForm form = MakeSimpleForm();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  std::vector<PasswordForm> observed = {form};
  manager()->OnPasswordFormsParsed(nullptr, observed);

  // Provisionally save and simulate a successful landing page load to make
  // manager() believe this password should be saved.
  manager()->OnPasswordFormSubmitted(nullptr, form);
  manager()->OnPasswordFormsRendered(nullptr, {}, true);

  // Destroy |manager_| to send off UKM metrics.
  manager_.reset();

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      GetMetricEntry(test_ukm_recorder,
                     ukm::builders::PasswordForm::kEntryName),
      ukm::builders::PasswordForm::kParsingOnSavingDifferenceName));
}
#endif

TEST_F(PasswordManagerTest, NoSavePromptWhenPasswordManagerDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);

  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  manager()->OnPasswordFormsParsed(&driver_, {form});

  auto submitted_form = form;
  submitted_form.form_data.fields[0].value = ASCIIToUTF16("username");
  submitted_form.form_data.fields[1].value = ASCIIToUTF16("strong_password");

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormSubmittedNoChecks(&driver_, submitted_form);
}

TEST_F(PasswordManagerTest, NoSavePromptForNotPasswordForm) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);

  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  // Make the form to be credit card form.
  form.form_data.fields[1].autocomplete_attribute = "cc-csc";

  manager()->OnPasswordFormsParsed(&driver_, {form});

  auto submitted_form = form;
  submitted_form.form_data.fields[0].value = ASCIIToUTF16("text");
  submitted_form.form_data.fields[1].value = ASCIIToUTF16("1234");

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
  manager()->OnPasswordFormSubmittedNoChecks(&driver_, submitted_form);
}

// Check that when autofill predictions are received before a form is found then
// server predictions are not ignored and used for filling.
TEST_F(PasswordManagerTest, AutofillPredictionBeforeFormParsed) {
  NewPasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_.get());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewPasswordFormParsing);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  // Simulate that the form is incorrectly marked as sign-up, which means it can
  // not be filled without server predictions.
  form.form_data.fields[1].autocomplete_attribute = "new-password";

  // Server predictions says that this is a sign-in form. Since they have higher
  // priority than autocomplete attributes then the form should be filled.
  FormStructure form_structure(form.form_data);
  form_structure.field(1)->set_server_type(autofill::PASSWORD);
  manager()->ProcessAutofillPredictions(&driver_, {&form_structure});

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeConsumer(form)));
  // There are 2 fills, the first when the server predictions are received, the
  // second when the filling delayed task is executed. In production code a
  // delayed task is not posted since receiving results from the store is
  // asynchronous in contrast to test code.
  EXPECT_CALL(driver_, FillPasswordForm(_)).Times(2);

  manager()->OnPasswordFormsParsed(&driver_, {form});
  task_runner_->FastForwardUntilNoTasksRemain();
}

// Checks the following scenario:
// 1. The user is typing in a password form.
// 2. Navigation happens.
// 3. The password disappeared after navigation.
// 4. A save prompt is shown.
TEST_F(PasswordManagerTest, SavingAfterUserTypingAndNavigation) {
  for (bool form_may_be_submitted : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "form_may_be_submitted = " << form_may_be_submitted);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list, true);

    PasswordForm form(MakeSimpleForm());
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.origin))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
    manager()->OnPasswordFormsParsed(&driver_, {form});

    // The user is typing as a result the saving manual fallback is shown.
    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    manager()->ShowManualFallbackForSaving(&driver_, form);
    ASSERT_TRUE(form_manager_to_save);
    EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
                FormMatches(form));

    // Check that a save prompt is shown when there is no password form after
    // the navigation (which suggests that the submission was successful).
    if (form_may_be_submitted) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
          .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    }

    manager()->DidNavigateMainFrame(form_may_be_submitted);
    manager()->OnPasswordFormsRendered(&driver_, {}, true);

    EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
                FormMatches(form));
    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

// Check that when a form is submitted and a NewPasswordFormManager not present,
// this ends up reported in ProvisionallySaveFailure UMA and UKM.
TEST_F(PasswordManagerTest, ProvisionallySaveFailure) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  for (bool new_parsing_for_saving : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "new_parsing_for_saving = " << new_parsing_for_saving);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list, new_parsing_for_saving);

    manager()->OnPasswordFormsParsed(nullptr, {});

    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    auto metrics_recorder = std::make_unique<PasswordManagerMetricsRecorder>(
        1234, GURL("http://example.com"));
    EXPECT_CALL(client_, GetMetricsRecorder())
        .WillRepeatedly(Return(metrics_recorder.get()));

    PasswordForm unobserved_form = MakeSimpleForm();
    manager()->OnPasswordFormSubmitted(nullptr, unobserved_form);

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.ProvisionalSaveFailure",
        PasswordManagerMetricsRecorder::NO_MATCHING_FORM, 1);
    // Flush the UKM reports.
    EXPECT_CALL(client_, GetMetricsRecorder()).WillRepeatedly(Return(nullptr));
    metrics_recorder.reset();
    CheckMetricHasValue(
        test_ukm_recorder, ukm::builders::PageWithPassword::kEntryName,
        ukm::builders::PageWithPassword::kProvisionalSaveFailureName,
        PasswordManagerMetricsRecorder::NO_MATCHING_FORM);
  }
}

namespace {

// A convenience helper for type conversions.
template <typename T>
base::Optional<int64_t> MetricValue(T value) {
  return base::Optional<int64_t>(static_cast<int64_t>(value));
}

struct MissingFormManagerTestCase {
  // Description for logging.
  const char* const description = nullptr;
  // Is Chrome allowed to save passwords?
  enum class Saving { Enabled, Disabled } saving = Saving::Enabled;
  // What signal does Chrome have for saving?
  enum class Signal { Automatic, Manual, None } save_signal = Signal::Automatic;
  // All the forms which are parsed at once.
  std::vector<PasswordForm> parsed_forms;
  // A list of forms to be processed for saving, one at a time.
  std::vector<PasswordForm> processed_forms;
  // The expected value of the PageWithPassword::kFormManagerAvailableName
  // metric, or base::nullopt if no value should be logged.
  base::Optional<int64_t> expected_metric_value;

  bool run_for_old_parser = true;
  bool run_for_new_parser = true;
};

}  // namespace

// Test that presence of form managers in various situations is appropriately
// reported through UKM.
TEST_F(PasswordManagerTest, ReportMissingFormManager) {
  const PasswordForm form = MakeSimpleForm();
  PasswordForm other_form = MakeSimpleForm();
  ++other_form.form_data.unique_renderer_id;
  other_form.signon_realm += "other";

  const MissingFormManagerTestCase kTestCases[] = {
      {
          .description =
              "A form is submitted and a NewPasswordFormManager not present.",
          .parsed_forms = {},
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          // .parsed_forms is empty, so the processed form below was not
          // observed and has no form manager associated.
          .processed_forms = {form},
          .expected_metric_value =
              MetricValue(PasswordManagerMetricsRecorder::FormManagerAvailable::
                              kMissingProvisionallySave),
      },
      {
          .description = "Manual saving is requested and a "
                         "NewPasswordFormManager not present.",
          .parsed_forms = {},
          .save_signal = MissingFormManagerTestCase::Signal::Manual,
          // .parsed_forms is empty, so the processed form below was not
          // observed and has no form manager associated.
          .processed_forms = {form},
          .expected_metric_value =
              MetricValue(PasswordManagerMetricsRecorder::FormManagerAvailable::
                              kMissingManual),
          .run_for_new_parser = false,
      },
      {
          .description = "Manual saving is requested and a "
                         "NewPasswordFormManager is created.",
          .parsed_forms = {},
          .save_signal = MissingFormManagerTestCase::Signal::Manual,
          // .parsed_forms is empty, so the processed form below was not
          // observed and has no form manager associated.
          .processed_forms = {form},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
          .run_for_old_parser = false,
      },
      {
          .description = "Manual saving is successfully requested.",
          .parsed_forms = {form},
          .save_signal = MissingFormManagerTestCase::Signal::Manual,
          .processed_forms = {form},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description =
              "A form is submitted and a NewPasswordFormManager present.",
          .parsed_forms = {form},
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .processed_forms = {form},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description = "First failure, then success.",
          .parsed_forms = {form},
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          // Processing |other_form| first signals a failure value in the
          // metric, but processing |form| after that should overwrite that with
          // kSuccess.
          .processed_forms = {other_form, form},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description = "No forms, no report.",
          .parsed_forms = {},
          .save_signal = MissingFormManagerTestCase::Signal::None,
          .processed_forms = {},
          .expected_metric_value = base::nullopt,
      },
      {
          .description = "Not enabled, no report.",
          .saving = MissingFormManagerTestCase::Saving::Disabled,
          .parsed_forms = {form},
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .processed_forms = {form},
          .expected_metric_value = base::nullopt,
      },
  };

  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));
  for (const MissingFormManagerTestCase& test_case : kTestCases) {
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(test_case.saving ==
                               MissingFormManagerTestCase::Saving::Enabled));
    std::vector<bool> only_new_parser_options(1, true);
#if !defined(OS_IOS)
    // The old parser is not present on iOS anymore.
    only_new_parser_options.push_back(false);
#endif

    for (bool only_new_parser : only_new_parser_options) {
      if ((only_new_parser && !test_case.run_for_new_parser) ||
          (!only_new_parser && !test_case.run_for_old_parser)) {
        continue;
      }
      SCOPED_TRACE(testing::Message()
                   << "test case = " << test_case.description
                   << ", only_new_parser = " << only_new_parser);
      base::test::ScopedFeatureList scoped_feature_list;
      if (only_new_parser)
        TurnOnOnlyNewParser(&scoped_feature_list);
      else
        TurnOnNewParsingForFilling(&scoped_feature_list, true);

      manager()->OnPasswordFormsParsed(nullptr, test_case.parsed_forms);

      ukm::TestAutoSetUkmRecorder test_ukm_recorder;
      auto metrics_recorder = std::make_unique<PasswordManagerMetricsRecorder>(
          1234, GURL("http://example.com"));
      EXPECT_CALL(client_, GetMetricsRecorder())
          .WillRepeatedly(Return(metrics_recorder.get()));

      for (const PasswordForm& processed_form : test_case.processed_forms) {
        switch (test_case.save_signal) {
          case MissingFormManagerTestCase::Signal::Automatic:
            manager()->OnPasswordFormSubmitted(nullptr, processed_form);
            break;
          case MissingFormManagerTestCase::Signal::Manual:
            manager()->ShowManualFallbackForSaving(nullptr, processed_form);
            break;
          case MissingFormManagerTestCase::Signal::None:
            break;
        }
      }

      // Flush the UKM reports.
      EXPECT_CALL(client_, GetMetricsRecorder())
          .WillRepeatedly(Return(nullptr));
      metrics_recorder.reset();
      if (test_case.expected_metric_value) {
        CheckMetricHasValue(
            test_ukm_recorder, ukm::builders::PageWithPassword::kEntryName,
            ukm::builders::PageWithPassword::kFormManagerAvailableName,
            test_case.expected_metric_value.value());
      } else {
        EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
            GetMetricEntry(test_ukm_recorder,
                           ukm::builders::PageWithPassword::kEntryName),
            ukm::builders::PageWithPassword::kFormManagerAvailableName));
      }
    }
  }
}

// Tests that despite there a form was not seen on a page load, new
// |NewPasswordFormManager| is created in process of saving.
TEST_F(PasswordManagerTest, CreateNewPasswordFormManagerOnSaving) {
  base::test::ScopedFeatureList scoped_feature_list;
  TurnOnNewParsingForSaving(&scoped_feature_list, true);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(*store_, GetLogins(_, _))
      .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

  manager()->OnPasswordFormsParsed(&driver_, {form});

  // Simulate that JavaScript creates a new form, fills username/password and
  // submits it.
  auto submitted_form = form;
  submitted_form.form_data.unique_renderer_id += 1000;
  submitted_form.username_value = ASCIIToUTF16("username1");
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.password_value = ASCIIToUTF16("password1");
  submitted_form.form_data.fields[1].value = submitted_form.password_value;

  OnPasswordFormSubmitted(submitted_form);
  EXPECT_TRUE(manager()->GetSubmittedManagerForTest());

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_))
      .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));

  // The form disappeared, so the submission is condered to be successful.
  manager()->OnPasswordFormsRendered(&driver_, {}, true);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(submitted_form));
}

// Tests that no save prompt from form manager is shown when Credentials
// Management API function store is called.
TEST_F(PasswordManagerTest, NoSavePromptAfterStoreCalled) {
  for (bool new_parsing_for_saving : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "new_parsing_for_saving = " << new_parsing_for_saving);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list, new_parsing_for_saving);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(true));

    PasswordForm form(MakeSimpleForm());
    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeEmptyConsumerWithForms()));

    manager()->OnPasswordFormsParsed(&driver_, {form});

    // Simulate that navigator.credentials.store function is called.
    manager()->NotifyStorePasswordCalled();

    OnPasswordFormSubmitted(form);
    EXPECT_FALSE(manager()->GetSubmittedManagerForTest());
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);

    manager()->OnPasswordFormsRendered(&driver_, {}, true);
    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

// Check that on non-password form, saving and filling fallbacks are available
// but no automatic filling and saving are available.
TEST_F(PasswordManagerTest, FillingAndSavingFallbacksOnNonPasswordForm) {
  NewPasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  for (bool new_parsing_for_saving : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "is_new_parsing_on = " << new_parsing_for_saving);
    base::test::ScopedFeatureList scoped_feature_list;
    TurnOnNewParsingForSaving(&scoped_feature_list, new_parsing_for_saving);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(true));

    PasswordForm saved_match(MakeSimpleForm());
    PasswordForm credit_card_form(MakeSimpleCreditCardForm());
    credit_card_form.only_for_fallback = true;

    EXPECT_CALL(*store_, GetLogins(_, _))
        .WillRepeatedly(WithArg<1>(InvokeConsumer(saved_match)));

    PasswordFormFillData form_data;
    EXPECT_CALL(driver_, FillPasswordForm(_)).WillOnce(SaveArg<0>(&form_data));

    manager()->OnPasswordFormsParsed(&driver_, {credit_card_form});
    // Check that manual filling fallback available.
    EXPECT_EQ(saved_match.username_value, form_data.username_field.value);
    EXPECT_EQ(saved_match.password_value, form_data.password_field.value);
    // Check that no automatic filling available.
    uint32_t renderer_id_not_set = FormFieldData::kNotSetFormControlRendererId;
    EXPECT_EQ(renderer_id_not_set, form_data.username_field.unique_renderer_id);
    EXPECT_EQ(renderer_id_not_set, form_data.password_field.unique_renderer_id);

    // Check that saving fallback is available.
    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, ShowManualFallbackForSavingPtr(_, false, false))
        .WillOnce(WithArg<0>(SaveToScopedPtr(&form_manager_to_save)));
    manager()->ShowManualFallbackForSaving(&driver_, credit_card_form);
    ASSERT_TRUE(form_manager_to_save);
    EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
                FormMatches(credit_card_form));

    // Check that no automatic save prompt is shown.
    OnPasswordFormSubmitted(credit_card_form);
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePasswordPtr(_)).Times(0);
    manager()->DidNavigateMainFrame(true);
    manager()->OnPasswordFormsRendered(&driver_, {}, true);

    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

}  // namespace password_manager
