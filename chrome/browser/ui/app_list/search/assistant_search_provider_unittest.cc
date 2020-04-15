// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace test {

// Aliases.
using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
using AssistantSuggestionPtr =
    chromeos::assistant::mojom::AssistantSuggestionPtr;

// Expectations ----------------------------------------------------------------

class Expect {
 public:
  explicit Expect(const ChromeSearchResult& result) : r_(result) {
    EXPECT_EQ(r_.display_index(), ash::SearchResultDisplayIndex::kFirstIndex);
    EXPECT_EQ(r_.display_type(), ash::SearchResultDisplayType::kChip);
    EXPECT_EQ(r_.result_type(), ash::AppListSearchResultType::kAssistantChip);
    EXPECT_EQ(r_.GetSearchResultType(), ash::SearchResultType::ASSISTANT);
    EXPECT_TRUE(r_.chip_icon().BackedBySameObjectAs(gfx::CreateVectorIcon(
        ash::kAssistantIcon,
        ash::AppListConfig::instance().suggestion_chip_icon_dimension(),
        gfx::kPlaceholderColor)));
  }

  Expect(const Expect&) = delete;
  Expect& operator=(const Expect&) = delete;
  ~Expect() = default;

  Expect& Matches(const AssistantSuggestion& starter) {
    EXPECT_EQ(r_.id(), "googleassistant://" + starter.id.ToString());
    EXPECT_EQ(r_.title(), base::UTF8ToUTF16(starter.text));
    return *this;
  }

 private:
  const ChromeSearchResult& r_;
};

// ConversationStarterBuilder --------------------------------------------------

class ConversationStarterBuilder {
 public:
  ConversationStarterBuilder() = default;
  ConversationStarterBuilder(const ConversationStarterBuilder&) = delete;
  ConversationStarterBuilder& operator=(const ConversationStarterBuilder&) =
      delete;
  ~ConversationStarterBuilder() = default;

  AssistantSuggestionPtr Build() {
    DCHECK(!id_.is_empty());
    DCHECK(!text_.empty());

    AssistantSuggestionPtr conversation_starter = AssistantSuggestion::New();
    conversation_starter->id = id_;
    conversation_starter->text = text_;
    return conversation_starter;
  }

  ConversationStarterBuilder& WithId(const base::UnguessableToken& id) {
    id_ = id;
    return *this;
  }

  ConversationStarterBuilder& WithText(const std::string& text) {
    text_ = text;
    return *this;
  }

 private:
  base::UnguessableToken id_;
  std::string text_;
};

// TestAssistantSuggestionsController ------------------------------------------

class TestAssistantSuggestionsController
    : public ash::AssistantSuggestionsController {
 public:
  TestAssistantSuggestionsController() {
    SetConversationStarter(ConversationStarterBuilder()
                               .WithId(base::UnguessableToken::Create())
                               .WithText("Initial result")
                               .Build());
  }

  TestAssistantSuggestionsController(
      const TestAssistantSuggestionsController&) = delete;
  TestAssistantSuggestionsController& operator=(
      const TestAssistantSuggestionsController&) = delete;
  ~TestAssistantSuggestionsController() override = default;

  // ash::AssistantSuggestionsController:
  const ash::AssistantSuggestionsModel* GetModel() const override {
    return &model_;
  }

  void AddModelObserver(
      ash::AssistantSuggestionsModelObserver* observer) override {
    model_.AddObserver(observer);
  }

  void RemoveModelObserver(
      ash::AssistantSuggestionsModelObserver* observer) override {
    model_.RemoveObserver(observer);
  }

  void ClearConversationStarters() { SetConversationStarters({}); }

  void SetConversationStarter(AssistantSuggestionPtr conversation_starter) {
    std::vector<AssistantSuggestionPtr> conversation_starters;
    conversation_starters.push_back(std::move(conversation_starter));
    SetConversationStarters(std::move(conversation_starters));
  }

  void SetConversationStarters(
      std::vector<AssistantSuggestionPtr> conversation_starters) {
    model_.SetConversationStarters(std::move(conversation_starters));
  }

 private:
  ash::AssistantSuggestionsModel model_;
};

// AssistantSearchProviderTest -------------------------------------------------

class AssistantSearchProviderTest : public AppListTestBase {
 public:
  AssistantSearchProviderTest() = default;
  AssistantSearchProviderTest(const AssistantSearchProviderTest&) = delete;
  AssistantSearchProviderTest& operator=(const AssistantSearchProviderTest&) =
      delete;
  ~AssistantSearchProviderTest() override = default;

  AssistantSearchProvider& search_provider() { return search_provider_; }

  TestAssistantSuggestionsController& suggestions_controller() {
    return suggestions_controller_;
  }

 private:
  TestAssistantSuggestionsController suggestions_controller_;
  AssistantSearchProvider search_provider_;
};

// Tests -----------------------------------------------------------------------

TEST_F(AssistantSearchProviderTest, ShouldHaveAnInitialResult) {
  std::vector<const AssistantSuggestion*> conversation_starters =
      suggestions_controller().GetModel()->GetConversationStarters();

  ASSERT_EQ(1u, conversation_starters.size());
  ASSERT_EQ(1u, search_provider().results().size());

  const ChromeSearchResult& result = *search_provider().results().at(0);
  Expect(result).Matches(*conversation_starters.front());
}

TEST_F(AssistantSearchProviderTest, ShouldClearResultsDynamically) {
  EXPECT_EQ(1u, search_provider().results().size());

  suggestions_controller().ClearConversationStarters();
  EXPECT_TRUE(search_provider().results().empty());
}

TEST_F(AssistantSearchProviderTest, ShouldUpdateResultsDynamically) {
  AssistantSuggestionPtr update = ConversationStarterBuilder()
                                      .WithId(base::UnguessableToken::Create())
                                      .WithText("Updated result")
                                      .Build();

  suggestions_controller().SetConversationStarter(update->Clone());
  ASSERT_EQ(1u, search_provider().results().size());

  const ChromeSearchResult& result = *search_provider().results().at(0);
  Expect(result).Matches(*update);
}

}  // namespace test
}  // namespace app_list
