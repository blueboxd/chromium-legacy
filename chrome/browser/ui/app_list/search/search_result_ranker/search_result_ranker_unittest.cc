// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_result_ranker.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_data.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/data_decoder/public/cpp/test_data_decoder_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ResultType = ash::SearchResultType;

using base::ScopedTempDir;
using base::test::ScopedFeatureList;
using testing::ElementsAre;
using testing::WhenSorted;

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, ResultType type)
      : instance_id_(instantiation_count++) {
    set_id(id);
    SetTitle(base::UTF8ToUTF16(id));
    SetResultType(type);
  }
  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
  void InvokeAction(int action_index, int event_flags) override {}
  SearchResultType GetSearchResultType() const override {
    return app_list::SEARCH_RESULT_TYPE_BOUNDARY;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

MATCHER_P(HasId, id, "") {
  return base::UTF16ToUTF8(arg.result->title()) == id;
}

}  // namespace

class SearchResultRankerTest : public testing::Test {
 public:
  SearchResultRankerTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SearchResultRankerTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_ = profile_builder.Build();
  }

  std::unique_ptr<SearchResultRanker> MakeRanker(
      bool query_based_mixed_types_enabled,
      const std::map<std::string, std::string>& params = {}) {
    if (query_based_mixed_types_enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          app_list_features::kEnableQueryBasedMixedTypesRanker, params);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          app_list_features::kEnableQueryBasedMixedTypesRanker);
    }

    auto ranker = std::make_unique<SearchResultRanker>(profile_.get(),
                                                       dd_service_.connector());
    return ranker;
  }

  Mixer::SortedResults MakeSearchResults(const std::vector<std::string>& ids,
                                         const std::vector<ResultType>& types,
                                         const std::vector<double> scores) {
    Mixer::SortedResults results;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
      test_search_results_.emplace_back(ids[i], types[i]);
      results.emplace_back(&test_search_results_.back(), scores[i]);
    }
    return results;
  }

  void Wait() { thread_bundle_.RunUntilIdle(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  // This is used only to make the ownership clear for the TestSearchResult
  // objects that the return value of MakeSearchResults() contains raw pointers
  // to.
  std::list<TestSearchResult> test_search_results_;

  data_decoder::TestDataDecoderService dd_service_;

  ScopedFeatureList scoped_feature_list_;
  ScopedTempDir temp_dir_;

  std::unique_ptr<Profile> profile_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultRankerTest);
};

TEST_F(SearchResultRankerTest, MixedTypesRankersAreDisabledWithFlag) {
  auto ranker = MakeRanker(false);
  ranker->InitializeRankers();
  Wait();

  AppLaunchData app_launch_data;
  app_launch_data.id = "unused";
  app_launch_data.ranking_item_type = RankingItemType::kFile;
  app_launch_data.query = "query";

  for (int i = 0; i < 20; ++i)
    ranker->Train(app_launch_data);
  ranker->FetchRankings(base::string16());

  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kOmnibox, ResultType::kOmnibox,
                         ResultType::kLauncher, ResultType::kLauncher},
                        {0.6f, 0.5f, 0.4f, 0.3f});

  // Despite training, we expect the scores not to have changed.
  ranker->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("A"), HasId("B"),
                                              HasId("C"), HasId("D"))));
}

TEST_F(SearchResultRankerTest, CategoryModelImprovesScores) {
  auto ranker = MakeRanker(
      true, {{"use_category_model", "true"}, {"boost_coefficient", "1.0"}});
  ranker->InitializeRankers();
  Wait();

  AppLaunchData app_launch_data;
  app_launch_data.id = "unused";
  app_launch_data.ranking_item_type = RankingItemType::kFile;
  app_launch_data.query = "query";

  for (int i = 0; i < 20; ++i)
    ranker->Train(app_launch_data);
  ranker->FetchRankings(base::string16());

  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kOmnibox, ResultType::kOmnibox,
                         ResultType::kLauncher, ResultType::kLauncher},
                        {0.5f, 0.6f, 0.45f, 0.46f});

  ranker->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("D"), HasId("C"),
                                              HasId("B"), HasId("A"))));
}

TEST_F(SearchResultRankerTest, AppModelImprovesScores) {
  auto ranker = MakeRanker(false);
  ranker->InitializeRankers();
  Wait();

  AppLaunchData app_A;
  app_A.id = "A";
  app_A.ranking_item_type = RankingItemType::kApp;

  AppLaunchData app_B;
  app_B.id = "B";
  app_B.ranking_item_type = RankingItemType::kApp;

  for (int i = 0; i < 20; ++i) {
    ranker->Train(app_A);
    ranker->Train(app_B);
    ranker->Train(app_A);
  }
  ranker->FetchRankings(base::string16());

  auto results =
      MakeSearchResults({"A", "B", "C", "D"},
                        {ResultType::kInstalledApp, ResultType::kInstalledApp,
                         ResultType::kInstalledApp, ResultType::kInstalledApp},
                        {0.1f, 0.2f, 0.3f, 0.4f});

  ranker->Rank(&results);
  // The relevance scores put D > C > B > A, but we've trained on A the most,
  // B half as much, and C and D not at all. So we expect A > B > D > C.
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("A"), HasId("B"),
                                              HasId("D"), HasId("C"))));
}

TEST_F(SearchResultRankerTest, DefaultQueryMixedModelImprovesScores) {
  // Without the |use_category_model| parameter, the ranker defaults to the item
  // model.  With the |config| parameter, the ranker uses the default predictor
  // for the RecurrenceRanker.
  base::RunLoop run_loop;
  auto ranker = MakeRanker(true, {{"boost_coefficient", "1.0"}});
  ranker->set_json_config_parsed_for_testing(run_loop.QuitClosure());
  ranker->InitializeRankers();
  run_loop.Run();
  Wait();

  AppLaunchData app_launch_data_c;
  app_launch_data_c.id = "C";
  app_launch_data_c.ranking_item_type = RankingItemType::kFile;
  app_launch_data_c.query = "query";

  AppLaunchData app_launch_data_d;
  app_launch_data_d.id = "D";
  app_launch_data_d.ranking_item_type = RankingItemType::kFile;
  app_launch_data_d.query = "query";

  for (int i = 0; i < 10; ++i) {
    ranker->Train(app_launch_data_c);
    ranker->Train(app_launch_data_d);
  }
  ranker->FetchRankings(base::UTF8ToUTF16("query"));

  // The types associated with these results don't match what was trained on,
  // to check that the type is irrelevant to the item model.
  auto results = MakeSearchResults({"A", "B", "C", "D"},
                                   {ResultType::kOmnibox, ResultType::kOmnibox,
                                    ResultType::kOmnibox, ResultType::kOmnibox},
                                   {0.3f, 0.2f, 0.1f, 0.1f});

  ranker->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId("D"), HasId("C"),
                                              HasId("A"), HasId("B"))));
}

// URL IDs should ignore the query and fragment, and URLs for google docs should
// ignore a trailing /view or /edit.
TEST_F(SearchResultRankerTest, QueryMixedModelNormalizesUrlIds) {
  // We want |url_1| and |_3| to be equivalent to |url_2| and |_4|. So, train on
  // 1 and 3 but rank 2 and 4. Even with zero relevance, they should be at the
  // top of the rankings.
  const std::string& url_1 = "http://docs.google.com/mydoc/edit?query";
  const std::string& url_2 = "http://docs.google.com/mydoc/view#fragment";
  const std::string& url_3 = "some.domain.com?query#edit";
  const std::string& url_4 = "some.domain.com";

  base::RunLoop run_loop;
  auto ranker = MakeRanker(true, {{"boost_coefficient", "1.0"}});
  ranker->set_json_config_parsed_for_testing(run_loop.QuitClosure());
  ranker->InitializeRankers();
  run_loop.Run();
  Wait();

  AppLaunchData app_launch_data_1;
  app_launch_data_1.id = url_1;
  app_launch_data_1.ranking_item_type = RankingItemType::kOmniboxHistory;
  app_launch_data_1.query = "query";
  AppLaunchData app_launch_data_3;
  app_launch_data_3.id = url_3;
  app_launch_data_3.ranking_item_type = RankingItemType::kOmniboxHistory;
  app_launch_data_3.query = "query";

  for (int i = 0; i < 5; ++i) {
    ranker->Train(app_launch_data_1);
    ranker->Train(app_launch_data_3);
  }
  ranker->FetchRankings(base::UTF8ToUTF16("query"));

  auto results = MakeSearchResults(
      {url_2, url_4, "untrained id"},
      {ResultType::kOmnibox, ResultType::kOmnibox, ResultType::kOmnibox},
      {0.0f, 0.0f, 0.1f});

  ranker->Rank(&results);
  EXPECT_THAT(results, WhenSorted(ElementsAre(HasId(url_4), HasId(url_2),
                                              HasId("untrained id"))));
}

// Ensure that a JSON config deployed via Finch results in the correct model
// being constructed.
TEST_F(SearchResultRankerTest, QueryMixedModelConfigDeployment) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "exponential weights ensemble",
        "learning_rate": 1.6,
        "predictors": [
          {"predictor_type": "default"},
          {"predictor_type": "markov"},
          {"predictor_type": "frecency", "decay_coeff": 0.8}
        ]
      }
    })";

  base::RunLoop run_loop;
  auto ranker =
      MakeRanker(true, {{"boost_coefficient", "1.0"}, {"config", json}});
  ranker->set_json_config_parsed_for_testing(run_loop.QuitClosure());
  ranker->InitializeRankers();
  run_loop.Run();
  Wait();

  EXPECT_EQ(std::string(ranker->query_based_mixed_types_ranker_
                            ->GetPredictorNameForTesting()),
            "ExponentialWeightsEnsemble");
}

}  // namespace app_list
