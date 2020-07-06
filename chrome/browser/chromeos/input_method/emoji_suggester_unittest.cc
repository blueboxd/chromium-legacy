// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

const char kEmojiData[] = "happy,😀;😃;😄";

class TestSuggestionHandler : public SuggestionHandlerInterface {
 public:
  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override {
    return false;
  }

  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override {
    return false;
  }

  bool DismissSuggestion(int context_id, std::string* error) override {
    return false;
  }

  bool AcceptSuggestion(int context_id, std::string* error) override {
    return false;
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}

  bool ShowMultipleSuggestions(int context_id,
                               const std::vector<base::string16>& candidates,
                               std::string* error) override {
    return false;
  }

  bool AcceptSuggestionCandidate(int context_id,
                                 const base::string16& candidate,
                                 std::string* error) override {
    return false;
  }

  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override {
    return false;
  }
};

class EmojiSuggesterTest : public testing::Test {
 protected:
  void SetUp() override {
    engine_ = std::make_unique<TestSuggestionHandler>();
    emoji_suggester_ = std::make_unique<EmojiSuggester>(engine_.get());
    emoji_suggester_->LoadEmojiMapForTesting(kEmojiData);
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_enabled_for_test(false);
  }

  std::unique_ptr<EmojiSuggester> emoji_suggester_;
  std::unique_ptr<TestSuggestionHandler> engine_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpace) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
}

TEST_F(EmojiSuggesterTest, SuggestWhenStringEndsWithSpaceAndIsUppercase) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("HAPPY ")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringEndsWithNewLine) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy\n")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenStringDoesNotEndWithSpace) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy")));
}

TEST_F(EmojiSuggesterTest, DoNotSuggestWhenWordNotInMap) {
  EXPECT_FALSE(emoji_suggester_->Suggest(base::UTF8ToUTF16("hapy ")));
}

TEST_F(EmojiSuggesterTest, DoNotShowSuggestionWhenVirtualKeyboardEnabled) {
  chrome_keyboard_controller_client_->set_keyboard_enabled_for_test(true);
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  EXPECT_FALSE(emoji_suggester_->GetSuggestionShownForTesting());
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Down";
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkBrowsingWhenPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Up";
  EXPECT_EQ(SuggestionStatus::kBrowsing,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkDismissWhenPressingEsc) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Esc";
  EXPECT_EQ(SuggestionStatus::kDismiss,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest, ReturnkAcceptWhenPressDownThenValidNumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "1";
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenNumberNotInRange) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "4";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenNotANumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "a";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest, ReturnkNotHandledWhenPressDownThenUpThenANumber) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "Up";
  emoji_suggester_->HandleKeyEvent(event2);
  InputMethodEngineBase::KeyboardEvent event3;
  event3.key = "1";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event3));
}

TEST_F(EmojiSuggesterTest,
       ReturnkNotHandledWhenPressingEnterAndACandidateHasNotBeenChosen) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  InputMethodEngineBase::KeyboardEvent event;
  event.key = "Enter";
  EXPECT_EQ(SuggestionStatus::kNotHandled,
            emoji_suggester_->HandleKeyEvent(event));
}

TEST_F(EmojiSuggesterTest,
       ReturnkAcceptWhenPressingEnterAndACandidateHasBeenChosenByPressingDown) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press "Down" to choose a candidate.
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Down";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "Enter";
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

TEST_F(EmojiSuggesterTest,
       ReturnkAcceptWhenPressingEnterAndACandidateHasBeenChosenByPressingUp) {
  EXPECT_TRUE(emoji_suggester_->Suggest(base::UTF8ToUTF16("happy ")));
  // Press "Down" to choose a candidate.
  InputMethodEngineBase::KeyboardEvent event1;
  event1.key = "Up";
  emoji_suggester_->HandleKeyEvent(event1);
  InputMethodEngineBase::KeyboardEvent event2;
  event2.key = "Enter";
  EXPECT_EQ(SuggestionStatus::kAccept,
            emoji_suggester_->HandleKeyEvent(event2));
}

}  // namespace chromeos
