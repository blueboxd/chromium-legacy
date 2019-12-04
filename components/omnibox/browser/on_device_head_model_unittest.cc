// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_model.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Pair;

namespace {

// The test head model used for unittests contains 14 queries and their scores
// shown below; the test model uses 3-bytes address and 2-bytes score so the
// highest score is 32767:
// ----------------------
// Query            Score
// ----------------------
// g                32767
// gmail            32766
// google maps      32765
// google           32764
// get out          32763
// googler          32762
// gamestop         32761
// maps             32761
// mail             32760
// map              32759
// 谷歌              32759
// ガツガツしてる人    32759
// 비데 두꺼비         32759
// переводчик       32759
// ----------------------
// The tree structure for queries above is similar as this:
//  [ g | ma | 谷歌 | ガツガツしてる人| 비데 두꺼비 | переводчик ]
//    |   |
//    | [ p | il ]
//    |   |
//    | [ # | s ]
//    |
//  [ # | oogle | mail | et out | amestop ]
//          |
//        [ # | _maps | er ]

base::FilePath GetTestModelPath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.AppendASCII(
      "components/test/data/omnibox/on_device_head_test_model_index.bin");
  return file_path;
}

}  // namespace

class OnDeviceHeadModelTest : public testing::Test {
 protected:
  void SetUp() override {
    base::FilePath file_path = GetTestModelPath();
    ASSERT_TRUE(base::PathExists(file_path));
#if defined(OS_WIN)
    model_ = OnDeviceHeadModel::Create(base::WideToUTF8(file_path.value()), 4);
#else
    model_ = OnDeviceHeadModel::Create(file_path.value(), 4);
#endif
    ASSERT_TRUE(model_);
  }

  void TearDown() override { model_.reset(); }

  std::unique_ptr<OnDeviceHeadModel> model_;
};

TEST_F(OnDeviceHeadModelTest, SizeOfScoreAndAddress) {
  EXPECT_EQ((int)model_->num_bytes_of_score(), 2);
  EXPECT_EQ((int)model_->num_bytes_of_address(), 3);
}

TEST_F(OnDeviceHeadModelTest, GetSuggestions) {
  auto suggestions = model_->GetSuggestionsForPrefix("go");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("google maps", 32765), Pair("google", 32764),
                          Pair("googler", 32762)));

  suggestions = model_->GetSuggestionsForPrefix("ge");
  EXPECT_THAT(suggestions, ElementsAre(Pair("get out", 32763)));

  suggestions = model_->GetSuggestionsForPrefix("ga");
  EXPECT_THAT(suggestions, ElementsAre(Pair("gamestop", 32761)));
}

TEST_F(OnDeviceHeadModelTest, NoMatch) {
  auto suggestions = model_->GetSuggestionsForPrefix("x");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, MatchTheEndOfSuggestion) {
  auto suggestions = model_->GetSuggestionsForPrefix("ap");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, MatchAtTheMiddleOfSuggestion) {
  auto suggestions = model_->GetSuggestionsForPrefix("st");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, EmptyInput) {
  auto suggestions = model_->GetSuggestionsForPrefix("");
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(OnDeviceHeadModelTest, SetMaxSuggestionsToReturn) {
  model_->set_max_num_matches_to_return(5);
  auto suggestions = model_->GetSuggestionsForPrefix("g");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("g", 32767), Pair("gmail", 32766),
                          Pair("google maps", 32765), Pair("google", 32764),
                          Pair("get out", 32763)));

  model_->set_max_num_matches_to_return(2);
  suggestions = model_->GetSuggestionsForPrefix("ma");
  EXPECT_THAT(suggestions,
              ElementsAre(Pair("maps", 32761), Pair("mail", 32760)));
}

TEST_F(OnDeviceHeadModelTest, NonEnglishLanguage) {
  // Chinese.
  auto suggestions = model_->GetSuggestionsForPrefix("谷");
  EXPECT_THAT(suggestions, ElementsAre(Pair("谷歌", 32759)));

  // Japanese.
  suggestions = model_->GetSuggestionsForPrefix("ガツガツ");
  EXPECT_THAT(suggestions, ElementsAre(Pair("ガツガツしてる人", 32759)));

  // Korean.
  suggestions = model_->GetSuggestionsForPrefix("비데 ");
  EXPECT_THAT(suggestions, ElementsAre(Pair("비데 두꺼비", 32759)));

  // Russian.
  suggestions = model_->GetSuggestionsForPrefix("пере");
  EXPECT_THAT(suggestions, ElementsAre(Pair("переводчик", 32759)));
}
