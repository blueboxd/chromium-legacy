// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/highlighted_games_store.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/games/core/games_types.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"
#include "components/games/core/test/mocks.h"
#include "components/games/core/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

namespace {
GamesCatalog CreateCatalogWithTwoGames() {
  return test::CreateGamesCatalog(
      {test::CreateGame(/*id=*/1), test::CreateGame(/*id=*/2)});
}
}  // namespace

class HighlightedGamesStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    highlighted_games_store_ = std::make_unique<HighlightedGamesStore>(
        std::make_unique<test::MockDataFilesParser>());
    AssertCacheEmpty();
  }

  void AssertCacheEmpty() {
    base::Optional<Game> test_cache =
        highlighted_games_store_->TryGetFromCache();
    ASSERT_FALSE(test_cache.has_value());
  }

  // TaskEnvironment is used instead of SingleThreadTaskEnvironment since we
  // post a task to the thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<HighlightedGamesStore> highlighted_games_store_;
  base::FilePath fake_install_dir_ =
      base::FilePath(FILE_PATH_LITERAL("some/path"));
};

TEST_F(HighlightedGamesStoreTest, ProcessAsync_Success_WithCache) {
  GamesCatalog fake_catalog = CreateCatalogWithTwoGames();

  base::RunLoop run_loop;

  // We'll use the barrier closure to make sure both the pending callback and
  // the done callbacks were invoked upon success.
  auto barrier_closure = base::BarrierClosure(
      2, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&barrier_closure, &fake_catalog](ResponseCode code, const Game game) {
        // For now, we're only returning the first game from the catalog.
        EXPECT_TRUE(test::AreProtosEqual(fake_catalog.games().at(0), game));
        EXPECT_EQ(ResponseCode::kSuccess, code);
        barrier_closure.Run();
      }));

  highlighted_games_store_->ProcessAsync(fake_install_dir_, fake_catalog,
                                         barrier_closure);

  run_loop.Run();

  // Now the game should be cached.
  auto test_cache = highlighted_games_store_->TryGetFromCache();
  EXPECT_TRUE(test_cache);
  EXPECT_TRUE(
      test::AreProtosEqual(fake_catalog.games().at(0), test_cache.value()));
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_InvalidData) {
  GamesCatalog empty_catalog;
  base::RunLoop run_loop;

  // We'll use the barrier closure to make sure both the pending callback and
  // the done callbacks were invoked upon success.
  auto barrier_closure = base::BarrierClosure(
      2, base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&barrier_closure](ResponseCode code, const Game game) {
        EXPECT_TRUE(test::AreProtosEqual(Game(), game));
        EXPECT_EQ(ResponseCode::kInvalidData, code);
        barrier_closure.Run();
      }));

  highlighted_games_store_->ProcessAsync(fake_install_dir_, empty_catalog,
                                         barrier_closure);

  run_loop.Run();

  // Cache should be empty.
  AssertCacheEmpty();
}

TEST_F(HighlightedGamesStoreTest, ProcessAsync_NoCallback_Caches) {
  GamesCatalog fake_catalog = CreateCatalogWithTwoGames();

  base::RunLoop run_loop;

  highlighted_games_store_->ProcessAsync(
      fake_install_dir_, fake_catalog,
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));

  run_loop.Run();

  // Even if we didn't have any pending callback, the game should now be cached.
  auto test_cache = highlighted_games_store_->TryGetFromCache();
  EXPECT_TRUE(test_cache);
  EXPECT_TRUE(
      test::AreProtosEqual(fake_catalog.games().at(0), test_cache.value()));
}

TEST_F(HighlightedGamesStoreTest, HandleCatalogFailure_CallsCallback) {
  bool callback_called;
  ResponseCode expected_code = ResponseCode::kMissingCatalog;
  highlighted_games_store_->SetPendingCallback(base::BindLambdaForTesting(
      [&expected_code, &callback_called](ResponseCode code, const Game game) {
        EXPECT_EQ(expected_code, code);
        EXPECT_TRUE(test::AreProtosEqual(Game(), game));
        callback_called = true;
      }));

  highlighted_games_store_->HandleCatalogFailure(expected_code);

  EXPECT_TRUE(callback_called);
}

TEST_F(HighlightedGamesStoreTest, HandleCatalogFailure_NoCallback) {
  // No exception should be thrown.
  highlighted_games_store_->HandleCatalogFailure(ResponseCode::kMissingCatalog);
}

}  // namespace games
