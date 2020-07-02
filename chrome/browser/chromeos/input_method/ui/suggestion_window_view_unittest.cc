// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ui {
namespace ime {

class MockAssistiveDelegate : public AssistiveDelegate {
 public:
  ~MockAssistiveDelegate() override = default;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) const override {}
};

class SuggestionWindowViewTest : public views::ViewsTestBase {
 public:
  SuggestionWindowViewTest() {}
  ~SuggestionWindowViewTest() override {}

 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    InitCandidates();

    suggestion_window_view_ =
        new SuggestionWindowView(GetContext(), delegate_.get());
    candidate_button_.id = ButtonId::kSuggestion;
    suggestion_window_view_->InitWidget();
  }

  void TearDown() override {
    suggestion_window_view_->GetWidget()->CloseNow();
    views::ViewsTestBase::TearDown();
  }

  void InitCandidates() {
    for (int i = 0; i < 3; i++) {
      std::string candidate = base::NumberToString(i);
      candidates_.push_back(base::UTF8ToUTF16(candidate));
    }
  }

  size_t GetHighlightedCount() const {
    const auto& children =
        suggestion_window_view_->GetCandidateAreaForTesting()->children();
    return std::count_if(
        children.cbegin(), children.cend(),
        [](const views::View* v) { return !!v->background(); });
  }

  int GetHighlightedIndex() const {
    const auto& children =
        suggestion_window_view_->GetCandidateAreaForTesting()->children();
    const auto it =
        std::find_if(children.cbegin(), children.cend(),
                     [](const views::View* v) { return !!v->background(); });
    return (it == children.cend()) ? kInvalid
                                   : std::distance(children.cbegin(), it);
  }

  SuggestionWindowView* suggestion_window_view_;
  std::unique_ptr<MockAssistiveDelegate> delegate_ =
      std::make_unique<MockAssistiveDelegate>();
  std::vector<base::string16> candidates_;
  AssistiveWindowButton candidate_button_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionWindowViewTest);
};

TEST_F(SuggestionWindowViewTest, HighlightOneCandidateWhenIndexIsValid) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  for (int index = 0; index < static_cast<int>(candidates_.size()); index++) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(index, GetHighlightedIndex());
  }
}

TEST_F(SuggestionWindowViewTest, HighlightNoCandidateWhenIndexIsInvalid) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int invalid[] = {kInvalid, candidates_.size()};
  for (int index : invalid) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

    EXPECT_EQ(0u, GetHighlightedCount());
    EXPECT_EQ(kInvalid, GetHighlightedIndex());
  }
}

TEST_F(SuggestionWindowViewTest, HighlightTheSameCandidateWhenCalledTwice) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest,
       HighlightValidCandidateAfterGivingInvalidIndexThenValidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int valid_index = 0;
  candidate_button_.index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest,
       KeepHighlightingValidCandidateWhenGivingValidThenInvalidIndex) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int valid_index = 0;
  candidate_button_.index = valid_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = candidates_.size();
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(valid_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest, UnhighlightCandidateIfCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  candidate_button_.index = 0;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(0u, GetHighlightedCount());
  EXPECT_EQ(kInvalid, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest,
       DoesNotUnhighlightCandidateIfNotCurrentlyHighlighted) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);
  candidate_button_.index = -1;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(highlight_index, GetHighlightedIndex());
}

TEST_F(SuggestionWindowViewTest, DoesNotUnhighlightCandidateIfOutOfRange) {
  suggestion_window_view_->ShowMultipleCandidates(candidates_);
  int highlight_index = 0;
  candidate_button_.index = highlight_index;
  suggestion_window_view_->SetButtonHighlighted(candidate_button_, true);

  int invalid[] = {kInvalid, candidates_.size()};

  for (int index : invalid) {
    candidate_button_.index = index;
    suggestion_window_view_->SetButtonHighlighted(candidate_button_, false);

    EXPECT_EQ(1u, GetHighlightedCount());
    EXPECT_EQ(highlight_index, GetHighlightedIndex());
  }
}

}  // namespace ime
}  // namespace ui
