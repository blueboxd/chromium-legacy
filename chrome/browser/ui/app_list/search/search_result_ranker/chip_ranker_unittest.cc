// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"

#include <list>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::UnorderedElementsAre;
using testing::WhenSorted;

namespace app_list {
namespace {

using ResultType = ash::AppListSearchResultType;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, ResultType type)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetResultType(type);

    switch (type) {
      case ResultType::kFileChip:
      case ResultType::kDriveQuickAccessChip:
        SetDisplayType(DisplayType::kChip);
        break;
      case ResultType::kInstalledApp:
        // Apps that should be in the chips
        SetDisplayType(DisplayType::kTile);
        SetIsRecommendation(true);
        break;
      case ResultType::kPlayStoreApp:
        // Apps that shouldn't be in the chips
        SetDisplayType(DisplayType::kTile);
        break;
      default:
        SetDisplayType(DisplayType::kList);
        break;
    }
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  ash::SearchResultType GetSearchResultType() const override {
    return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};

int TestSearchResult::instantiation_count = 0;

MATCHER_P(HasId, id, "") {
  bool match = base::UTF16ToUTF8(arg.result->title()) == id;
  if (!match)
    *result_listener << "HasId wants '" << id << "', but got '"
                     << arg.result->title() << "'";
  return match;
}

MATCHER_P(HasScore, score, "") {
  const double tol = 1e-10;
  bool match = abs(arg.score - score) < tol;
  if (!match)
    *result_listener << "HasScore wants '" << score << "', but got '"
                     << arg.score << "'";
  return match;
}

}  // namespace

class ChipRankerTest : public testing::Test {
 public:
  ChipRankerTest() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();

    ranker_ = std::make_unique<ChipRanker>(profile_.get());
    task_environment_.RunUntilIdle();
    SetRankerModel();
  }

  ~ChipRankerTest() override = default;

  void SetRankerModel() {
    // Set up fake ranker model.
    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(240u);
    config.set_condition_limit(100u);
    config.set_condition_decay(0.99f);
    config.set_target_limit(500u);
    config.set_target_decay(0.99f);
    config.mutable_predictor()->mutable_fake_predictor();

    ranker_->SetForTest(std::make_unique<RecurrenceRanker>(
        "", profile_->GetPath().AppendASCII("test_chip_ranker.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile_.get())));
  }

  Mixer::SortedResults MakeSearchResults(const std::vector<std::string>& ids,
                                         const std::vector<ResultType>& types,
                                         const std::vector<double> scores) {
    Mixer::SortedResults results;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
      results_.emplace_back(ids[i], types[i]);
      results.emplace_back(&results_.back(), scores[i]);
    }
    return results;
  }

  void TrainRanker(const std::vector<std::string>& ids,
                   const std::vector<RankingItemType>& types,
                   const std::vector<int>& n,
                   bool reset = true) {
    // Reset ranker
    if (reset) {
      SetRankerModel();
      task_environment_.RunUntilIdle();
    }

    std::list<AppLaunchData> data;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
      data.emplace_back();
      data.back().id = ids[i];
      data.back().ranking_item_type = types[i];
    }

    int i = 0;
    for (const auto& d : data) {
      for (int j = 0; j < n[i]; ++j) {
        ranker_->Train(d);
      }
      ++i;
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;

  std::unique_ptr<ChipRanker> ranker_;

  // This is used only to make the ownership clear for the TestSearchResult
  // objects that the return value of MakeSearchResults() contains raw pointers
  // to.
  std::list<TestSearchResult> results_;
};

// Check that ranking an empty list has no effect.
TEST_F(ChipRankerTest, EmptyList) {
  Mixer::SortedResults results = MakeSearchResults({}, {}, {});
  ranker_->Rank(&results);
  EXPECT_EQ(results.size(), 0ul);
}

// Check that ranking only one app has no effect.
TEST_F(ChipRankerTest, OneAppOnly) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kFileChip, ResultType::kFileChip},
      {8.9, 0.7, 0.4});

  TrainRanker({"app1", "file1"},
              {RankingItemType::kApp, RankingItemType::kChip}, {1, 3});

  ranker_->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("file2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(0.7),
                                              HasScore(0.4))));
}

// Check that ranking only apps has no effect.
TEST_F(ChipRankerTest, AppsOnly) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "app3"},
                        {ResultType::kInstalledApp, ResultType::kPlayStoreApp,
                         ResultType::kInstalledApp},
                        {8.9, 8.8, 8.7});

  TrainRanker({"app1", "app2"}, {RankingItemType::kApp, RankingItemType::kApp},
              {1, 1});

  ranker_->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("app3"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.7))));
}

// Check that ranking only files has no effect.
TEST_F(ChipRankerTest, FilesOnly) {
  Mixer::SortedResults results = MakeSearchResults(
      {"file1", "file2", "file3"},
      {ResultType::kFileChip, ResultType::kDriveQuickAccessChip,
       ResultType::kFileChip},
      {0.9, 0.6, 0.4});

  TrainRanker({"file1", "file2"},
              {RankingItemType::kChip, RankingItemType::kChip}, {1, 1});

  ranker_->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("file2"),
                                              HasId("file3"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(0.9), HasScore(0.6),
                                              HasScore(0.4))));
}

// Check that ranking a non-chip result does not affect its score.
TEST_F(ChipRankerTest, UnchangedItem) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "omni1", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kOmnibox, ResultType::kFileChip},
                        {8.9, 8.7, 0.8, 0.7});

  TrainRanker({"app1", "app2", "omni1", "file1"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kOmniboxGeneric, RankingItemType::kChip},
              {3, 1, 1, 2});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("app2"), HasId("omni1"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.7), HasScore(0.8))));
}

// Check moving a file into first place.
TEST_F(ChipRankerTest, FileFirst) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kFileChip},
                        {8.8, 8.7, 0.9});

  TrainRanker(
      {"app1", "app2", "file1"},
      {RankingItemType::kApp, RankingItemType::kApp, RankingItemType::kChip},
      {1, 1, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("app1"),
                                              HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.7))));
}

// Check moving a file between two apps.
TEST_F(ChipRankerTest, FileMid) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kFileChip},
                        {8.9, 8.6, 0.8});

  TrainRanker(
      {"app1", "app2", "file1"},
      {RankingItemType::kApp, RankingItemType::kApp, RankingItemType::kChip},
      {3, 1, 2});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.75),
                                              HasScore(8.6))));
}

// Check moving a file into last place.
TEST_F(ChipRankerTest, FileLast) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kFileChip},
                        {8.9, 8.6, 0.4});

  TrainRanker(
      {"app1", "app2", "file1"},
      {RankingItemType::kApp, RankingItemType::kApp, RankingItemType::kChip},
      {2, 2, 1});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("file1"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.6),
                                              HasScore(0.4))));
}

// Check alternating app and file results.
TEST_F(ChipRankerTest, ChipAlternate) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kDriveQuickAccessChip, ResultType::kFileChip},
      {8.9, 8.6, 0.8, 0.3});

  TrainRanker({"app1", "app2", "file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {4, 2, 3, 1});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("app2"), HasId("file2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.75),
                                              HasScore(8.6), HasScore(0.3))));
}

// Check moving two files into first place.
TEST_F(ChipRankerTest, TwoFilesFirst) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kDriveQuickAccessChip, ResultType::kFileChip},
      {8.8, 8.6, 0.9, 0.8});

  TrainRanker({"app1", "app2", "file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {1, 2, 4, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("file2"),
                                              HasId("app1"), HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.85),
                                              HasScore(8.8), HasScore(8.6))));
}

// Check ranking a file that the ranker hasn't seen.
TEST_F(ChipRankerTest, UntrainedFile) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kDriveQuickAccessChip},
                        {8.8, 8.6, 0.9});

  TrainRanker({"app1", "app2"}, {RankingItemType::kApp, RankingItemType::kApp},
              {2, 1});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("file1"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.8), HasScore(8.6),
                                              HasScore(0.9))));
}

// Check that input order of apps remains the same even where ranker
// would swap them.
TEST_F(ChipRankerTest, AppMaintainOrder) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kDriveQuickAccessChip},
                        {8.8, 8.6, 0.9});

  TrainRanker(
      {"app1", "app2", "file1"},
      {RankingItemType::kApp, RankingItemType::kApp, RankingItemType::kChip},
      {1, 2, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("app1"),
                                              HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.6))));
}

// Check that file ordering remains the same even where ranker would
// swap them, when files are placed ahead of first app.
TEST_F(ChipRankerTest, FileMaintainOrderFirst) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kDriveQuickAccessChip, ResultType::kFileChip},
      {8.8, 8.6, 0.9, 0.8});

  TrainRanker({"app1", "app2", "file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {1, 2, 4, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("file2"),
                                              HasId("app1"), HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.85),
                                              HasScore(8.8), HasScore(8.6))));
}

// Check that file ordering remains the same even where ranker would
// swap them, when files are placed ahead of first app.
TEST_F(ChipRankerTest, FileMaintainOrderMid) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kDriveQuickAccessChip, ResultType::kFileChip},
      {8.8, 8.6, 0.9, 0.8});

  TrainRanker({"app1", "app2", "file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {4, 1, 2, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("file2"), HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.8), HasScore(8.7),
                                              HasScore(8.65), HasScore(8.6))));
}

// Check that rank works on a succession of training instances.
TEST_F(ChipRankerTest, TrainMultiple) {
  Mixer::SortedResults results = MakeSearchResults(
      {"app1", "app2", "file1", "file2"},
      {ResultType::kInstalledApp, ResultType::kInstalledApp,
       ResultType::kDriveQuickAccessChip, ResultType::kFileChip},
      {8.8, 8.6, 0.9, 0.8});

  TrainRanker({"app1", "app2", "file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {5, 2, 3, 1});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("file1"),
                                              HasId("app2"), HasId("file2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.8), HasScore(8.7),
                                              HasScore(8.6), HasScore(0.8))));

  TrainRanker({"file1", "file2"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {3, 2}, /* reset = */ false);

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("app1"),
                                              HasId("file2"), HasId("app2"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.9), HasScore(8.8),
                                              HasScore(8.7), HasScore(8.6))));
}

// Check ranker behaviour when multiple apps have equal scores.
TEST_F(ChipRankerTest, EqualApps) {
  Mixer::SortedResults results =
      MakeSearchResults({"app1", "app2", "app3", "file1"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kInstalledApp, ResultType::kFileChip},
                        {8.7, 8.7, 8.7, 0.5});

  TrainRanker({"app1", "app2", "app3", "file1"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {5, 2, 1, 3});

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("app1"), HasId("app2"),
                                              HasId("app3"), HasId("file1"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.7), HasScore(8.7),
                                              HasScore(8.7), HasScore(8.7))));

  TrainRanker({"file1"},
              {RankingItemType::kApp, RankingItemType::kApp,
               RankingItemType::kChip, RankingItemType::kChip},
              {3}, /* reset = */ false);

  ranker_->Rank(&results);

  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("file1"), HasId("app1"),
                                              HasId("app2"), HasId("app3"))));
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasScore(8.85), HasScore(8.7),
                                              HasScore(8.7), HasScore(8.7))));
}

}  // namespace app_list
