// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class TestSuggestionHandler : public SuggestionHandlerInterface {
 public:
  bool DismissSuggestion(int context_id, std::string* error) override {
    suggestion_text_ = base::EmptyString16();
    previous_suggestions_.clear();
    confirmed_length_ = 0;
    suggestion_accepted_ = false;
    return true;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    suggestion_text_ = details.text;
    confirmed_length_ = details.confirmed_length;
    show_annotation_ = details.show_annotation;
    show_setting_link_ = details.show_setting_link;
    return true;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    suggestion_text_ = base::EmptyString16();
    confirmed_length_ = 0;
    suggestion_accepted_ = true;
    return true;
  }

  void VerifySuggestion(const base::string16 text,
                        const size_t confirmed_length) {
    EXPECT_EQ(suggestion_text_, text);
    EXPECT_EQ(confirmed_length_, confirmed_length);
    suggestion_text_ = base::EmptyString16();
    confirmed_length_ = 0;
  }

  void VerifySuggestionDispatchedToExtension(
      const std::vector<std::string>& suggestions) {
    EXPECT_THAT(previous_suggestions_, testing::ContainerEq(suggestions));
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {
    previous_suggestions_ = suggestions;
  }

  bool ShowMultipleSuggestions(int context_id,
                               const std::vector<base::string16>& candidates,
                               std::string* error) override {
    return false;
  }

  void ClickButton(const ui::ime::AssistiveWindowButton& button) override {}

  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override {
    return false;
  }

  bool AcceptSuggestionCandidate(int context_id,
                                 const base::string16& candidate,
                                 std::string* error) override {
    return false;
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    return false;
  }

  void VerifyShowAnnotation(const bool show_annotation) {
    EXPECT_EQ(show_annotation_, show_annotation);
  }
  void VerifyShowSettingLink(const bool show_setting_link) {
    EXPECT_EQ(show_setting_link_, show_setting_link);
  }

  bool IsSuggestionAccepted() { return suggestion_accepted_; }

 private:
  base::string16 suggestion_text_;
  size_t confirmed_length_ = 0;
  bool show_annotation_ = false;
  bool show_setting_link_ = false;
  bool suggestion_accepted_ = false;
  std::vector<std::string> previous_suggestions_;
};

class TestTtsHandler : public TtsHandler {
 public:
  explicit TestTtsHandler(Profile* profile) : TtsHandler(profile) {}

  void VerifyAnnouncement(const std::string& expected_text) {
    EXPECT_EQ(text_, expected_text);
  }

 private:
  void Speak(const std::string& text) override { text_ = text; }

  std::string text_ = "";
};

}  // namespace

class PersonalInfoSuggesterTest : public testing::Test {
 protected:
  PersonalInfoSuggesterTest() {
    autofill_client_.SetPrefs(autofill::test::PrefServiceForTesting());
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto tts_handler = std::make_unique<TestTtsHandler>(profile_.get());
    tts_handler_ = tts_handler.get();

    suggestion_handler_ = std::make_unique<TestSuggestionHandler>();

    personal_data_ = std::make_unique<autofill::TestPersonalDataManager>();
    personal_data_->SetPrefService(autofill_client_.GetPrefs());

    suggester_ = std::make_unique<PersonalInfoSuggester>(
        suggestion_handler_.get(), profile_.get(), personal_data_.get(),
        std::move(tts_handler));

    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_enabled_for_test(false);
  }

  void SendKeyboardEvent(std::string key) {
    InputMethodEngineBase::KeyboardEvent event;
    event.key = key;
    suggester_->HandleKeyEvent(event);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  TestTtsHandler* tts_handler_;
  std::unique_ptr<TestSuggestionHandler> suggestion_handler_;
  std::unique_ptr<PersonalInfoSuggester> suggester_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  autofill::TestAutofillClient autofill_client_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_;

  const base::string16 email_ = base::UTF8ToUTF16("johnwayne@me.xyz");
  const base::string16 first_name_ = base::UTF8ToUTF16("John");
  const base::string16 last_name_ = base::UTF8ToUTF16("Wayne");
  const base::string16 full_name_ = base::UTF8ToUTF16("John Wayne");
  const base::string16 address_ =
      base::UTF8ToUTF16("1 Dream Road, Hollywood, CA 12345");
  const base::string16 phone_number_ = base::UTF8ToUTF16("16505678910");
};

TEST_F(PersonalInfoSuggesterTest, SuggestEmail) {
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifySuggestion(email_, 0);
}

TEST_F(PersonalInfoSuggesterTest, DoNotSuggestWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_enabled_for_test(true);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
}

TEST_F(PersonalInfoSuggesterTest,
       SendsEmailSuggestionToExtensionWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_enabled_for_test(true);
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifySuggestionDispatchedToExtension(
      std::vector<std::string>{base::UTF16ToUTF8(email_)});
}

TEST_F(PersonalInfoSuggesterTest, SuggestNames) {
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                              first_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST, last_name_);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->Suggest(base::UTF8ToUTF16("my first name is "));
  suggestion_handler_->VerifySuggestion(first_name_, 0);
  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");

  suggester_->Suggest(base::UTF8ToUTF16("my last name is "));
  suggestion_handler_->VerifySuggestion(last_name_, 0);
  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");

  suggester_->Suggest(base::UTF8ToUTF16("my name is "));
  suggestion_handler_->VerifySuggestion(full_name_, 0);
}

TEST_F(PersonalInfoSuggesterTest, SuggestAddress) {
  autofill::CountryNames::SetLocaleString("en-US");
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_LINE1,
                              base::UTF8ToUTF16("1 Dream Road"));
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_CITY,
                              base::UTF8ToUTF16("Hollywood"));
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_ZIP,
                              base::UTF8ToUTF16("12345"));
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_STATE,
                              base::UTF8ToUTF16("CA"));
  autofill_profile.SetRawInfo(autofill::ServerFieldType::ADDRESS_HOME_COUNTRY,
                              base::UTF8ToUTF16("US"));
  personal_data_->AddProfile(autofill_profile);

  suggester_->Suggest(base::UTF8ToUTF16("my address is "));
  suggestion_handler_->VerifySuggestion(address_, 0);
}

TEST_F(PersonalInfoSuggesterTest, SuggestPhoneNumber) {
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->Suggest(base::UTF8ToUTF16("my phone number is "));
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");

  suggester_->Suggest(base::UTF8ToUTF16("my number is "));
  suggestion_handler_->VerifySuggestion(phone_number_, 0);
}

TEST_F(PersonalInfoSuggesterTest, AcceptSuggestion) {
  profile_->set_profile_name(base::UTF16ToUTF8(email_));

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");

  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_TRUE(suggestion_handler_->IsSuggestionAccepted());
}

TEST_F(PersonalInfoSuggesterTest, DismissSuggestion) {
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL, full_name_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->Suggest(base::UTF8ToUTF16("my name is "));
  SendKeyboardEvent("Esc");
  suggestion_handler_->VerifySuggestion(base::EmptyString16(), 0);
  EXPECT_FALSE(suggestion_handler_->IsSuggestionAccepted());
}

TEST_F(PersonalInfoSuggesterTest, SuggestWithConfirmedLength) {
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER, phone_number_);
  personal_data_->AddProfile(autofill_profile);

  suggester_->Suggest(base::UTF8ToUTF16("my phone number is "));
  suggester_->Suggest(base::UTF8ToUTF16("my phone number is 16"));
  suggestion_handler_->VerifySuggestion(phone_number_, 2);
}

TEST_F(PersonalInfoSuggesterTest,
       DoNotAnnounceSpokenFeedbackWhenChromeVoxIsOff) {
  profile_->set_profile_name(base::UTF16ToUTF8(email_));
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, false);

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5000));
  tts_handler_->VerifyAnnouncement("");

  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5000));
  tts_handler_->VerifyAnnouncement("");
}

TEST_F(PersonalInfoSuggesterTest, AnnounceSpokenFeedbackWhenChromeVoxIsOn) {
  profile_->set_profile_name(base::UTF16ToUTF8(email_));
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  tts_handler_->VerifyAnnouncement("");

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));
  tts_handler_->VerifyAnnouncement(base::StringPrintf(
      "Suggestion %s. Press down to navigate and enter to insert.",
      base::UTF16ToUTF8(email_).c_str()));

  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(200));
  tts_handler_->VerifyAnnouncement(base::StringPrintf(
      "Inserted suggestion %s.", base::UTF16ToUTF8(email_).c_str()));
}

TEST_F(PersonalInfoSuggesterTest, DoNotShowAnnotationAfterMaxAcceptanceCount) {
  for (int i = 0; i < kMaxAcceptanceCount; i++) {
    suggester_->Suggest(base::UTF8ToUTF16("my email is "));
    SendKeyboardEvent("Down");
    SendKeyboardEvent("Enter");
    suggestion_handler_->VerifyShowAnnotation(true);
  }
  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifyShowAnnotation(false);
}

TEST_F(PersonalInfoSuggesterTest, DoNotAnnounceAnnotationWhenTabNotShown) {
  profile_->set_profile_name(base::UTF16ToUTF8(email_));
  profile_->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetIntKey(kPersonalInfoSuggesterAcceptanceCount, kMaxAcceptanceCount);

  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifyShowAnnotation(false);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(500));
  tts_handler_->VerifyAnnouncement("");

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));
  tts_handler_->VerifyAnnouncement(
      base::StringPrintf("Suggestion %s. ", base::UTF16ToUTF8(email_).c_str()));
}

TEST_F(PersonalInfoSuggesterTest, ShowSettingLink) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->RemoveKey(kPersonalInfoSuggesterShowSettingCount);
  update->RemoveKey(kPersonalInfoSuggesterAcceptanceCount);
  for (int i = 0; i < kMaxShowSettingCount; i++) {
    suggester_->Suggest(base::UTF8ToUTF16("my email is "));
    // Dismiss suggestion.
    SendKeyboardEvent("Esc");
    suggestion_handler_->VerifyShowSettingLink(true);
  }
  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifyShowSettingLink(false);
}

TEST_F(PersonalInfoSuggesterTest, DoNotShowSettingLinkAfterAcceptance) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kAssistiveInputFeatureSettings);
  update->SetIntKey(kPersonalInfoSuggesterShowSettingCount, 0);
  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifyShowSettingLink(true);
  // Accept suggestion.
  SendKeyboardEvent("Down");
  SendKeyboardEvent("Enter");
  suggester_->Suggest(base::UTF8ToUTF16("my email is "));
  suggestion_handler_->VerifyShowSettingLink(false);
}

}  // namespace chromeos
