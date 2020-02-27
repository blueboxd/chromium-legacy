// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"
#include "base/test/bind_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream_background.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

class TestFeedNetwork : public FeedNetwork {
 public:
  void SendQueryRequest(
      const feedwire::Request& request,
      base::OnceCallback<void(QueryRequestResult)> callback) override {}
  void SendActionRequest(
      const feedwire::ActionRequest& request,
      base::OnceCallback<void(ActionRequestResult)> callback) override {}
  void CancelRequests() override {}
};

class FeedStreamTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    // TODO(harringtond): Use the feed shared prefs registration function
    // when it exists.
    profile_prefs_.registry()->RegisterBooleanPref(prefs::kEnableSnippets,
                                                   true);
    profile_prefs_.registry()->RegisterBooleanPref(prefs::kArticlesListVisible,
                                                   true);

    stream_ = std::make_unique<FeedStream>(
        &profile_prefs_, &network_, &clock_, &tick_clock_,
        task_environment_.GetMainThreadTaskRunner());
  }

 protected:
  TestingPrefServiceSimple profile_prefs_;
  TestFeedNetwork network_;
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<FeedStream> stream_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FeedStreamTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, SetArticlesListVisible) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(false);
  EXPECT_FALSE(stream_->IsArticlesListVisible());
  stream_->SetArticlesListVisible(true);
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedStreamTest, RunInBackgroundAndReturn) {
  int result_received = 0;
  FeedStreamBackground* background_received = nullptr;
  base::RunLoop run_loop;
  auto quit = run_loop.QuitClosure();

  auto background_lambda =
      base::BindLambdaForTesting([&](FeedStreamBackground* bg) {
        background_received = bg;
        return 5;
      });
  auto result_lambda = base::BindLambdaForTesting([&](int result) {
    result_received = result;
    quit.Run();
  });

  stream_->RunInBackgroundAndReturn(FROM_HERE,
                                    base::BindOnce(background_lambda),
                                    base::BindOnce(result_lambda));

  run_loop.Run();

  EXPECT_TRUE(background_received);
  EXPECT_EQ(5, result_received);
}

}  // namespace
}  // namespace feed
