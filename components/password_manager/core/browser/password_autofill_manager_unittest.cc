// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kInvalidUsername[] = "no-username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";
const char kAliceAccountStoredPassword[] = "account-stored-password";
const char kAliceEmail[] = "alice@gmail.com";

using autofill::PopupType;
using autofill::Suggestion;
using autofill::SuggestionVectorIconsAre;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorLabelsAre;
using autofill::SuggestionVectorValuesAre;
using favicon_base::FaviconImageCallback;
using gfx::test::AreImagesEqual;
using testing::_;
using testing::AllOf;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SizeIs;
using testing::Unused;

using UkmEntry = ukm::builders::PageWithPassword;

namespace autofill {
class AutofillPopupDelegate;
}

namespace password_manager {

namespace {

using ReauthSucceeded = PasswordManagerClient::ReauthSucceeded;

constexpr char kMainFrameUrl[] = "https://example.com/";
constexpr char kDropdownSelectedHistogram[] =
    "PasswordManager.PasswordDropdownItemSelected";
constexpr char kDropdownShownHistogram[] =
    "PasswordManager.PasswordDropdownShown";
const gfx::Image kTestFavicon = gfx::test::CreateImage(16, 16);

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD2(PreviewSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient() : main_frame_url_(kMainFrameUrl) {}
  ~TestPasswordManagerClient() override = default;

  MockPasswordManagerDriver* mock_driver() { return &driver_; }
  const GURL& GetMainFrameURL() const override { return main_frame_url_; }

  const MockPasswordFeatureManager* GetPasswordFeatureManager() const override {
    return feature_manager_.get();
  }

  MockPasswordFeatureManager* GetPasswordFeatureManager() {
    return feature_manager_.get();
  }

  signin::IdentityManager* GetIdentityManager() override {
    return identity_test_env_.identity_manager();
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  void SetAccountStorageOptIn(bool opt_in) {
    ON_CALL(*feature_manager_.get(), ShouldShowAccountStorageOptIn)
        .WillByDefault(Return(!opt_in));
  }

  MOCK_METHOD0(GeneratePassword, void());
  MOCK_METHOD2(TriggerReauthForAccount,
               void(const CoreAccountId&,
                    base::OnceCallback<void(ReauthSucceeded)>));
  MOCK_METHOD0(GetFaviconService, favicon::FaviconService*());
  MOCK_METHOD1(NavigateToManagePasswordsPage, void(ManagePasswordsReferrer));

 private:
  MockPasswordManagerDriver driver_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockPasswordFeatureManager> feature_manager_{
      new NiceMock<MockPasswordFeatureManager>};
  GURL main_frame_url_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MOCK_METHOD6(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<Suggestion>& suggestions,
                    bool autoselect_first_suggestion,
                    PopupType popup_type,
                    base::WeakPtr<autofill::AutofillPopupDelegate> delegate));
  MOCK_METHOD0(PinPopupView, void());
  MOCK_CONST_METHOD0(GetPopupSuggestions,
                     base::span<const autofill::Suggestion>());
  MOCK_METHOD2(UpdatePopup,
               void(const std::vector<autofill::Suggestion>&, PopupType));
  MOCK_METHOD1(HideAutofillPopup, void(autofill::PopupHidingReason));
  MOCK_METHOD1(ExecuteCommand, void(int));
};

bool IsPreLollipopAndroid() {
#if defined(OS_ANDROID)
  return (base::android::BuildInfo::GetInstance()->sdk_int() <
          base::android::SDK_VERSION_LOLLIPOP);
#else
  return false;
#endif
}

std::vector<base::string16> GetSuggestionList(
    std::vector<base::string16> credentials) {
  if (!IsPreLollipopAndroid()) {
    credentials.push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  }
  return credentials;
}

std::vector<std::string> GetIconsList(std::vector<std::string> icons) {
  // On older Android versions the item "Manage passwords" is absent.
  if (!IsPreLollipopAndroid())
    icons.push_back(std::string());
  return icons;
}

base::CancelableTaskTracker::TaskId
RespondWithTestIcon(Unused, FaviconImageCallback callback, Unused) {
  favicon_base::FaviconImageResult image_result;
  image_result.image = kTestFavicon;
  std::move(callback).Run(image_result);
  return 1;
}

std::vector<autofill::Suggestion> CreateTestSuggestions(
    bool has_opt_in_and_fill,
    bool has_opt_in_and_generate) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(
      Suggestion(/*label=*/"User1", /*value=*/"PW1", /*icon=*/"",
                 /*fronend_id=*/autofill::POPUP_ITEM_ID_PASSWORD_ENTRY));
  if (!IsPreLollipopAndroid()) {
    suggestions.push_back(Suggestion(
        /*label=*/"Show all pwds", /*value=*/"", /*icon=*/"",
        /*fronend_id=*/autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY));
  }
  if (has_opt_in_and_fill) {
    suggestions.push_back(Suggestion(
        /*label=*/"Unlock passwords and fill", /*value=*/"", /*icon=*/"",
        /*fronend_id=*/
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN));
  }
  if (has_opt_in_and_generate) {
    suggestions.push_back(Suggestion(
        /*label=*/"Unlock passwords and generate", /*value=*/"", /*icon=*/"",
        /*fronend_id=*/
        autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE));
  }
  return suggestions;
}

std::vector<autofill::PopupItemId> RemoveShowAllBeforeLollipop(
    std::vector<autofill::PopupItemId> ids) {
  if (IsPreLollipopAndroid()) {
    ids.erase(
        std::remove_if(ids.begin(), ids.end(), [](autofill::PopupItemId id) {
          return id == autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY;
        }));
  }
  return ids;
}

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(base::ASCIIToUTF16(kAliceUsername)),
        test_password_(base::ASCIIToUTF16(kAlicePassword)) {}

  void SetUp() override {
    // Add a preferred login and an additional login to the FillData.
    autofill::FormFieldData username_field;
    username_field.name = base::ASCIIToUTF16(kUsernameName);
    username_field.value = test_username_;
    fill_data_.username_field = username_field;

    autofill::FormFieldData password_field;
    password_field.name = base::ASCIIToUTF16(kPasswordName);
    password_field.value = test_password_;
    fill_data_.password_field = password_field;
  }

  void InitializePasswordAutofillManager(TestPasswordManagerClient* client,
                                         MockAutofillClient* autofill_client) {
    password_autofill_manager_.reset(new PasswordAutofillManager(
        client->mock_driver(), autofill_client, client));
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(*client, GetFaviconService())
        .WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service,
                GetFaviconImageForPageURL(fill_data_.origin, _, _));
    password_autofill_manager_->OnAddPasswordFillData(fill_data_);
    testing::Mock::VerifyAndClearExpectations(client);
    // Suppress the warnings in the tests.
    EXPECT_CALL(*client, GetFaviconService()).WillRepeatedly(Return(nullptr));
  }

  autofill::PasswordFormFillData CreateTestFormFillData() {
    autofill::PasswordFormFillData data;
    data.username_field.value = test_username_;
    data.password_field.value = test_password_;
    data.preferred_realm = "http://foo.com/";
    return data;
  }

 protected:
  autofill::PasswordFormFillData& fill_data() { return fill_data_; }

  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;

  base::string16 test_username_;
  base::string16 test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PasswordAutofillManagerTest, FillSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      base::ASCIIToUTF16(kInvalidUsername)));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->FillSuggestionForTest(test_username_));
}

TEST_F(PasswordAutofillManagerTest, PreviewSuggestion) {
  TestPasswordManagerClient client;
  InitializePasswordAutofillManager(&client, nullptr);

  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(test_username_, test_password_));
  EXPECT_TRUE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  EXPECT_CALL(*client.mock_driver(), PreviewSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      base::ASCIIToUTF16(kInvalidUsername)));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(
      password_autofill_manager_->PreviewSuggestionForTest(test_username_));
}

// Test that the popup is marked as visible after receiving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling and favicon data.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    data.uses_account_store = false;
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.origin, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify the suggestions.
    std::vector<Suggestion> suggestions;
    EXPECT_CALL(
        autofill_client,
        ShowAutofillPopup(
            _, _,
            SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
                {is_suggestion_on_password_field
                     ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                     : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                 autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY}))),
            /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _))
        .WillOnce(testing::SaveArg<2>(&suggestions));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, base::string16(), show_suggestion_options,
        gfx::RectF());
    ASSERT_GE(suggestions.size(), 1u);
    EXPECT_TRUE(AreImagesEqual(suggestions[0].custom_icon, kTestFavicon));

    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, test_password_));
    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    base::HistogramTester histograms;
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_,
        is_suggestion_on_password_field
            ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
            : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
        1);
    histograms.ExpectUniqueSample(
        kDropdownSelectedHistogram,
        metrics_util::PasswordDropdownSelectedOption::kPassword, 1);
  }
}

// Test that suggestions are filled correctly if account-stored and local
// suggestion have duplicates.
TEST_F(PasswordAutofillManagerTest,
       ExternalDelegateAccountStorePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_suggestion_on_password_field = "
                                    << is_suggestion_on_password_field);
    TestPasswordManagerClient client;
    NiceMock<MockAutofillClient> autofill_client;
    InitializePasswordAutofillManager(&client, &autofill_client);

    // Load filling data and account-stored duplicate with a different password.
    autofill::PasswordFormFillData data = CreateTestFormFillData();
    autofill::PasswordAndMetadata duplicate;
    duplicate.password = base::ASCIIToUTF16(kAliceAccountStoredPassword);
    duplicate.realm = data.preferred_realm;
    duplicate.uses_account_store = true;
    data.additional_logins[data.username_field.value] = duplicate;
    favicon::MockFaviconService favicon_service;
    EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
    EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.origin, _, _))
        .WillOnce(Invoke(RespondWithTestIcon));
    password_autofill_manager_->OnAddPasswordFillData(data);

    // Show the popup and verify local and account-stored suggestion coexist.
    std::vector<Suggestion> suggestions;
    EXPECT_CALL(
        autofill_client,
        ShowAutofillPopup(
            _, _,
            SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
                {is_suggestion_on_password_field
                     ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                     : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
                 is_suggestion_on_password_field
                     ? autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY
                     : autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY,
                 autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY}))),
            /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _))
        .WillOnce(testing::SaveArg<2>(&suggestions));
    password_autofill_manager_->OnShowPasswordSuggestions(
        base::i18n::RIGHT_TO_LEFT, base::string16(),
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0,
        gfx::RectF());
    ASSERT_GE(suggestions.size(), 2u);
    EXPECT_TRUE(AreImagesEqual(suggestions[0].custom_icon, kTestFavicon));
    EXPECT_TRUE(AreImagesEqual(suggestions[1].custom_icon, kTestFavicon));

    // When selecting the account-stored credential, make sure the filled
    // password belongs to the selected credential (and not to the first match).
    EXPECT_CALL(*client.mock_driver(),
                FillSuggestion(test_username_, duplicate.password));
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_,
        is_suggestion_on_password_field
            ? autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY
            : autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY,
        /*position=*/1);
  }
}

// Test that the popup is updated once account-stored suggestions are unlocked.
TEST_F(PasswordAutofillManagerTest, ShowOptInAndFillButton) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);

  // Show the popup and verify the suggestions.
  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          _, _,
          SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
              {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
               autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
               autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN}))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::string16(),
      autofill::SHOW_ALL | autofill::IS_PASSWORD_FIELD, gfx::RectF());
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndFillPutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock button into a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(
          SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
              {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
               autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
               autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN}))),
          PopupType::kPasswords))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(autofill_client, PinPopupView);
  EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _));
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false)));
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked.
TEST_F(PasswordAutofillManagerTest,
       ClickOnOptInAndGeneratePutsPopupInWaitingState) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup. The
  // update puts the unlock-to-generate button in a loading state.
  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(
          SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
              {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
               autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
               autofill::
                   POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE}))),
          PopupType::kPasswords))
      .WillOnce(testing::SaveArg<0>(&suggestions));
  EXPECT_CALL(autofill_client, PinPopupView);
  EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _));
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true)));
  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE, 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_TRUE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndFillUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);
  EXPECT_CALL(*client.GetPasswordFeatureManager(), SetAccountStorageOptIn)
      .Times(0);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false)));
  EXPECT_CALL(autofill_client, UpdatePopup);

  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, PinPopupView).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetPopupSuggestions)
        .WillOnce(Return(CreateTestSuggestions(
            /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false)));
    EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _))
        .WillOnce([](const auto& unused, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
                {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                 autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                 autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN}))),
            PopupType::kPasswords))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and generate" is clicked.
TEST_F(PasswordAutofillManagerTest, FailedOptInAndGenerateUpdatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  std::vector<autofill::Suggestion> suggestions;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);
  EXPECT_CALL(*client.GetPasswordFeatureManager(), SetAccountStorageOptIn)
      .Times(0);

  // Accepting a suggestion should trigger a call to update the popup.
  // First the popup enters the waiting state.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true)));
  EXPECT_CALL(autofill_client, UpdatePopup);

  // As soon as the waiting state is pending, the next update resets the popup.
  EXPECT_CALL(autofill_client, PinPopupView).WillOnce([&] {
    testing::Mock::VerifyAndClear(&autofill_client);
    EXPECT_CALL(autofill_client, GetPopupSuggestions)
        .WillOnce(Return(CreateTestSuggestions(
            /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true)));
    EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _))
        .WillOnce([](const auto& unused, auto reauth_callback) {
          std::move(reauth_callback).Run(ReauthSucceeded(false));
        });
    EXPECT_CALL(
        autofill_client,
        UpdatePopup(
            SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
                {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
                 autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY,
                 autofill::
                     POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE}))),
            PopupType::kPasswords))
        .WillOnce(testing::SaveArg<0>(&suggestions));
  });

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE, 1);
  ASSERT_GE(suggestions.size(), 2u);
  EXPECT_FALSE(suggestions.back().is_loading);
}

// Test that the popup is updated once "opt in and fill" is clicked.
TEST_F(PasswordAutofillManagerTest, SuccessfullOptInAndFillTriggersOptIn) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/true, /*has_opt_in_and_generate*/ false)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, PinPopupView);

  EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _))
      .WillOnce([](const auto& id, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(*client.GetPasswordFeatureManager(),
              SetAccountStorageOptIn(true));

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_, autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN,
      1);
}

// Test that the popup is updated once "opt in and generate" is clicked.
TEST_F(PasswordAutofillManagerTest,
       SuccessfullOptInAndGenerateTriggersOptInAndGeneration) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(false);
  const CoreAccountId kAliceId = client.identity_test_env()
                                     .SetUnconsentedPrimaryAccount(kAliceEmail)
                                     .account_id;
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Accepting a suggestion should trigger a call to update the popup.
  EXPECT_CALL(autofill_client, GetPopupSuggestions)
      .WillOnce(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ true)));
  EXPECT_CALL(autofill_client, UpdatePopup);
  EXPECT_CALL(autofill_client, PinPopupView);

  EXPECT_CALL(client, TriggerReauthForAccount(kAliceId, _))
      .WillOnce([](const auto& id, auto reauth_callback) {
        std::move(reauth_callback).Run(ReauthSucceeded(true));
      });
  EXPECT_CALL(*client.GetPasswordFeatureManager(),
              SetAccountStorageOptIn(true));
  EXPECT_CALL(client, GeneratePassword());

  password_autofill_manager_->DidAcceptSuggestion(
      test_username_,
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE, 1);
}

// Test that the popup is updated once "opt in and fill" is clicked".
TEST_F(PasswordAutofillManagerTest,
       AddOnFillDataAfterOptInAndFillPopulatesPopup) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);
  client.SetAccountStorageOptIn(true);
  testing::Mock::VerifyAndClearExpectations(&autofill_client);

  // Once the data is loaded, an update fills the new passwords:
  autofill::PasswordFormFillData new_data = CreateTestFormFillData();
  new_data.uses_account_store = true;
  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  new_data.additional_logins[additional_username] = additional;
  EXPECT_CALL(autofill_client, GetPopupSuggestions())
      .Times(2)
      .WillRepeatedly(Return(CreateTestSuggestions(
          /*has_opt_in_and_fill=*/false, /*has_opt_in_and_generate*/ false)));
  EXPECT_CALL(
      autofill_client,
      UpdatePopup(
          SuggestionVectorIdsAre(ElementsAreArray(RemoveShowAllBeforeLollipop(
              {autofill::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY,
               autofill::POPUP_ITEM_ID_PASSWORD_ENTRY,
               autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY}))),
          PopupType::kPasswords));
  password_autofill_manager_->OnAddPasswordFillData(new_data);
}

// Test that OnShowPasswordSuggestions correctly matches the given FormFieldData
// to the known PasswordFormFillData, and extracts the right suggestions.
TEST_F(PasswordAutofillManagerTest, ExtractSuggestions) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  // First, simulate displaying suggestions matching an empty prefix. Also
  // verify that both the values and labels are filled correctly. The 'value'
  // should be the user name; the 'label' should be the realm.
  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          testing::AllOf(
              SuggestionVectorValuesAre(testing::UnorderedElementsAreArray(
                  GetSuggestionList({test_username_, additional_username}))),
              SuggestionVectorLabelsAre(testing::AllOf(
                  testing::Contains(base::UTF8ToUTF16("foo.com")),
                  testing::Contains(base::UTF8ToUTF16("foobarrealm.org"))))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::string16(), 0, element_bounds);

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(ElementsAreArray(
                                    GetSuggestionList({additional_username}))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("John"), 0, element_bounds);

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(ElementsAreArray(
              GetSuggestionList({test_username_, additional_username}))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("xyz"), autofill::SHOW_ALL,
      element_bounds);
}

// Verify that, for Android application credentials, the prettified realms of
// applications are displayed as the labels of suggestions on the UI (for
// matches of all levels of preferredness).
TEST_F(PasswordAutofillManagerTest, PrettifiedAndroidRealmsAreShownAsLabels) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.preferred_realm = "android://hash@com.example1.android/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "android://hash@com.example2.android/";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(_, _,
                                SuggestionVectorLabelsAre(testing::AllOf(
                                    testing::Contains(base::ASCIIToUTF16(
                                        "android://com.example1.android/")),
                                    testing::Contains(base::ASCIIToUTF16(
                                        "android://com.example2.android/")))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::string16(), 0, gfx::RectF());
}

TEST_F(PasswordAutofillManagerTest, FillSuggestionPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(ElementsAreArray(
                                    GetSuggestionList({test_username_}))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_, autofill::IS_PASSWORD_FIELD,
      element_bounds);
}

// Verify that typing "foo" into the username field will match usernames
// "foo.bar@example.com", "bar.foo@example.com" and "example@foo.com".
TEST_F(PasswordAutofillManagerTest, DisplaySuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::UnorderedElementsAreArray(
              GetSuggestionList({username, additional_username}))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo"), 0, element_bounds);
}

// Verify that typing "oo" into the username field will not match any usernames
// "foo.bar@example.com", "bar.foo@example.com" or "example@foo.com".
TEST_F(PasswordAutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("oo"), 0, element_bounds);
}

// Verify that typing "foo@exam" into the username field will match username
// "bar.foo@example.com" even if the field contents span accross multiple
// tokens.
TEST_F(PasswordAutofillManagerTest,
       MatchingContentsWithSuggestionTokenSeparator) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(ElementsAreArray(
                                    GetSuggestionList({additional_username}))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo@exam"), 0,
      element_bounds);
}

// Verify that typing "example" into the username field will match and order
// usernames "example@foo.com", "foo.bar@example.com" and "bar.foo@example.com"
// i.e. prefix matched followed by substring matched.
TEST_F(PasswordAutofillManagerTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillTokenPrefixMatching);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndMetadata additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(ElementsAreArray(
              GetSuggestionList({username, additional_username}))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo"), false,
      element_bounds);
}

TEST_F(PasswordAutofillManagerTest, PreviewAndFillEmptyUsernameSuggestion) {
  // Initialize PasswordAutofillManager with credentials without username.
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  fill_data().username_field.value.clear();
  InitializePasswordAutofillManager(&client, &autofill_client);

  base::string16 no_username_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);

  // Simulate that the user clicks on a username field.
  EXPECT_CALL(autofill_client, ShowAutofillPopup);
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::string16(),
      /*autoselect_first_suggestion=*/false, element_bounds);

  // Check that preview of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              PreviewSuggestion(base::string16(), test_password_));
  password_autofill_manager_->DidSelectSuggestion(no_username_string,
                                                  0 /*not used*/);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());

  // Check that fill of the empty username works.
  EXPECT_CALL(*client.mock_driver(),
              FillSuggestion(base::string16(), test_password_));
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      no_username_string, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, 1);
  testing::Mock::VerifyAndClearExpectations(client.mock_driver());
}

// Tests that the "Manage passwords" suggestion is shown along with the password
// popup.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnPasswordField) {
  constexpr char kShownContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsShownContext";
  constexpr char kAcceptedContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsAcceptedContext";
  base::HistogramTester histograms;

  NiceMock<MockAutofillClient> autofill_client;
  auto client = std::make_unique<TestPasswordManagerClient>();
  auto manager = std::make_unique<PasswordManager>(client.get());
  InitializePasswordAutofillManager(client.get(), &autofill_client);

  ON_CALL(*(client->mock_driver()), GetPasswordManager())
      .WillByDefault(testing::Return(manager.get()));

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(ElementsAreArray(
                                    GetSuggestionList({test_username_}))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));

  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_, autofill::IS_PASSWORD_FIELD,
      element_bounds);
  histograms.ExpectUniqueSample(kDropdownShownHistogram,
                                metrics_util::PasswordDropdownState::kStandard,
                                1);

  if (!IsPreLollipopAndroid()) {
    // Expect a sample only in the shown histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::ShowAllSavedPasswordsContext::kPassword, 1);
    // Clicking at the "Show all passwords row" should trigger a call to open
    // the Password Manager settings page and hide the popup.
    EXPECT_CALL(*client, NavigateToManagePasswordsPage(
                             ManagePasswordsReferrer::kPasswordDropdown));
    EXPECT_CALL(
        autofill_client,
        HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
    password_autofill_manager_->DidAcceptSuggestion(
        base::string16(), autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY, 0);
    // Expect a sample in both the shown and accepted histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::ShowAllSavedPasswordsContext::kPassword, 1);
    histograms.ExpectUniqueSample(
        kAcceptedContextHistogram,
        metrics_util::ShowAllSavedPasswordsContext::kPassword, 1);
    histograms.ExpectUniqueSample(
        kDropdownSelectedHistogram,
        metrics_util::PasswordDropdownSelectedOption::kShowAll, 1);
    // Trigger UKM reporting, which happens at destruction time.
    ukm::SourceId expected_source_id = client->GetUkmSourceId();
    manager.reset();
    client.reset();

    const auto& entries =
        autofill_client.GetTestUkmRecorder()->GetEntriesByName(
            UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      EXPECT_EQ(expected_source_id, entry->source_id);
      ukm::TestUkmRecorder::ExpectEntryMetric(
          entry, UkmEntry::kPageLevelUserActionName,
          static_cast<int64_t>(
              PasswordManagerMetricsRecorder::PageLevelUserAction::
                  kShowAllPasswordsWhileSomeAreSuggested));
    }
  } else {
    EXPECT_THAT(histograms.GetAllSamples(kShownContextHistogram),
                testing::IsEmpty());
    EXPECT_THAT(histograms.GetAllSamples(kAcceptedContextHistogram),
                testing::IsEmpty());
  }
}

// Tests that the "Manage passwords" fallback shows up in non-password
// fields of login forms.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnNonPasswordField) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  password_autofill_manager_->OnAddPasswordFillData(data);

  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(ElementsAreArray(
                                    GetSuggestionList({test_username_}))),
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, test_username_, 0, element_bounds);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationNoCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  password_autofill_manager_.reset(new PasswordAutofillManager(
      client.mock_driver(), &autofill_client, &client));

  EXPECT_CALL(autofill_client, ShowAutofillPopup).Times(0);
  gfx::RectF element_bounds;
  EXPECT_FALSE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithGenerationSomeCredentials) {
  base::HistogramTester histograms;
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.origin, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  // Bring up the drop-down with the generaion option.
  base::string16 generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);
  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          AllOf(SuggestionVectorValuesAre(ElementsAreArray(
                    GetSuggestionList({test_username_, generation_string}))),
                SuggestionVectorIconsAre(
                    ElementsAreArray(GetIconsList({"globeIcon", "keyIcon"})))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));
  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/true));
  histograms.ExpectUniqueSample(
      kDropdownShownHistogram,
      metrics_util::PasswordDropdownState::kStandardGenerate, 1);

  // Click "Generate password".
  EXPECT_CALL(client, GeneratePassword());
  EXPECT_CALL(
      autofill_client,
      HideAutofillPopup(autofill::PopupHidingReason::kAcceptSuggestion));
  password_autofill_manager_->DidAcceptSuggestion(
      base::string16(), autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY, 1);
  histograms.ExpectUniqueSample(
      kDropdownSelectedHistogram,
      metrics_util::PasswordDropdownSelectedOption::kGenerate, 1);
}

TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithOmittedCredentials) {
  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.origin, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  base::string16 generation_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_GENERATE_PASSWORD);

  EXPECT_CALL(
      autofill_client,
      ShowAutofillPopup(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          AllOf(SuggestionVectorValuesAre(
                    ElementsAreArray(GetSuggestionList({generation_string}))),
                SuggestionVectorIconsAre(
                    ElementsAreArray(GetIconsList({"keyIcon"})))),
          /*autoselect_first_suggestion=*/false, PopupType::kPasswords, _));

  EXPECT_TRUE(
      password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
          element_bounds, base::i18n::RIGHT_TO_LEFT,
          /*show_password_suggestions=*/false));
}

// Test that if the "opt in and generate" button gets displayed, the regular
// generation button does not.
TEST_F(PasswordAutofillManagerTest,
       MaybeShowPasswordSuggestionsWithAccountPasswordsEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  client.SetAccountStorageOptIn(false);

  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  autofill::PasswordFormFillData data = CreateTestFormFillData();

  favicon::MockFaviconService favicon_service;
  EXPECT_CALL(client, GetFaviconService()).WillOnce(Return(&favicon_service));
  EXPECT_CALL(favicon_service, GetFaviconImageForPageURL(data.origin, _, _));
  password_autofill_manager_->OnAddPasswordFillData(data);

  auto opt_in_and_generate_id = static_cast<int>(
      autofill::POPUP_ITEM_ID_PASSWORD_ACCOUNT_STORAGE_OPT_IN_AND_GENERATE);
  auto regular_generate_id =
      static_cast<int>(autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY);
  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(
                  _, _,
                  AllOf(Contains(Field(&autofill::Suggestion::frontend_id,
                                       Eq(opt_in_and_generate_id))),
                        Not(Contains(Field(&autofill::Suggestion::frontend_id,
                                           Eq(regular_generate_id))))),
                  _, _, _));

  password_autofill_manager_->MaybeShowPasswordSuggestionsWithGeneration(
      gfx::RectF(), base::i18n::RIGHT_TO_LEFT,
      /*show_password_suggestions=*/true);
}

TEST_F(PasswordAutofillManagerTest, DisplayAccountSuggestionsIndicatorIcon) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kEnablePasswordsAccountStorage);

  TestPasswordManagerClient client;
  NiceMock<MockAutofillClient> autofill_client;
  InitializePasswordAutofillManager(&client, &autofill_client);

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.uses_account_store = true;

  password_autofill_manager_->OnAddPasswordFillData(data);

  std::vector<autofill::Suggestion> suggestions;
  EXPECT_CALL(autofill_client,
              ShowAutofillPopup(element_bounds, _, _,
                                /*autoselect_first_suggestion=*/false,
                                PopupType::kPasswords, _))
      .WillOnce(testing::SaveArg<2>(&suggestions));
  password_autofill_manager_->OnShowPasswordSuggestions(
      base::i18n::RIGHT_TO_LEFT, base::string16(), false, element_bounds);
  ASSERT_THAT(suggestions.size(), testing::Ge(1u));  // No footer on Android.
  EXPECT_THAT(suggestions[0].store_indicator_icon, "google");
}

}  // namespace password_manager
