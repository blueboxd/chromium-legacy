// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/games/core/games_service_impl.h"

#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/test/mocks.h"
#include "components/games/core/test/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace games {

class GamesServiceImplTest : public testing::Test {
 protected:
  void SetUp() override {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    games::prefs::RegisterProfilePrefs(test_pref_service_->registry());

    auto mock_catalog_store = std::make_unique<test::MockCatalogStore>();
    mock_catalog_store_ = mock_catalog_store.get();

    auto mock_hg_store = std::make_unique<test::MockHighlightedGamesStore>();
    mock_highlighted_games_store_ = mock_hg_store.get();

    games_service_ = std::make_unique<GamesServiceImpl>(
        std::move(mock_catalog_store), std::move(mock_hg_store),
        test_pref_service_.get());

    ASSERT_FALSE(games_service_->is_updating());
  }

  void SetInstallDirPref() {
    prefs::SetInstallDirPath(test_pref_service_.get(), fake_install_dir_);
  }

  void SetTryRespondFromCacheResponse(bool succeeds) {
    EXPECT_CALL(*mock_highlighted_games_store_, TryRespondFromCache())
        .WillOnce(Return(succeeds));
  }

  // TaskEnvironment is used instead of SingleThreadTaskEnvironment since we
  // post a task to the thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  test::MockCatalogStore* mock_catalog_store_;
  test::MockHighlightedGamesStore* mock_highlighted_games_store_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<GamesServiceImpl> games_service_;
  base::FilePath fake_install_dir_ =
      base::FilePath(FILE_PATH_LITERAL("some/path"));
};

TEST_F(GamesServiceImplTest, GetHighlightedGame_NotInstalled) {
  bool callback_invoked = false;
  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&callback_invoked](ResponseCode code, const Game game) {
        EXPECT_EQ(ResponseCode::kFileNotFound, code);
        test::ExpectProtosEqual(Game(), game);
        callback_invoked = true;
      }));

  EXPECT_TRUE(callback_invoked);
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_RetrievesFromCache) {
  // Mock component to be installed.
  SetInstallDirPref();

  // Expect the UI callback to have been given to the highlighted games store.
  EXPECT_CALL(*mock_highlighted_games_store_, SetPendingCallback(_)).Times(1);

  // Don't expect processing to be invoked as we'll have returned from cache.
  EXPECT_CALL(*mock_catalog_store_, UpdateCatalogAsync(_, _)).Times(0);
  EXPECT_CALL(*mock_highlighted_games_store_, ProcessAsync(_, _, _)).Times(0);

  SetTryRespondFromCacheResponse(true);

  games_service_->GetHighlightedGame(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // No-op.
      }));
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_Success) {
  SetInstallDirPref();
  SetTryRespondFromCacheResponse(false);

  // Expect the UI callback to have been given to the highlighted games store.
  EXPECT_CALL(*mock_highlighted_games_store_, SetPendingCallback(_)).Times(1);

  GamesCatalog fake_catalog = test::CreateGamesCatalogWithOneGame();

  // Mock that the catalog store parses and caches the catalog successfully.
  EXPECT_CALL(*mock_catalog_store_, UpdateCatalogAsync(fake_install_dir_, _))
      .WillOnce([&](const base::FilePath& install_dir,
                    base::OnceCallback<void(ResponseCode)> callback) {
        EXPECT_EQ(fake_install_dir_, install_dir);
        EXPECT_TRUE(games_service_->is_updating());

        // Set up cache at this point.
        mock_catalog_store_->set_cached_catalog(&fake_catalog);

        std::move(callback).Run(ResponseCode::kSuccess);
      });

  // Mock that the highlighted games store processes successfully and invokes
  // the done callback.
  EXPECT_CALL(*mock_highlighted_games_store_,
              ProcessAsync(fake_install_dir_, _, _))
      .WillOnce([&](const base::FilePath& install_dir,
                    const games::GamesCatalog& catalog,
                    base::OnceClosure done_callback) {
        EXPECT_TRUE(games_service_->is_updating());
        test::ExpectProtosEqual(fake_catalog, catalog);

        // Invoke the done callback to signal that the HighlightedStore is done
        // processing.
        std::move(done_callback).Run();
      });

  EXPECT_CALL(*mock_catalog_store_, ClearCache()).Times(1);

  games_service_->GetHighlightedGame(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // No-op.
      }));

  EXPECT_FALSE(games_service_->is_updating());
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_CatalogFileNotFound) {
  SetInstallDirPref();
  SetTryRespondFromCacheResponse(false);

  EXPECT_CALL(*mock_catalog_store_, UpdateCatalogAsync(fake_install_dir_, _))
      .WillOnce([](const base::FilePath& install_dir,
                   base::OnceCallback<void(ResponseCode)> callback) {
        std::move(callback).Run(ResponseCode::kFileNotFound);
      });

  EXPECT_CALL(*mock_highlighted_games_store_,
              HandleCatalogFailure(ResponseCode::kFileNotFound))
      .Times(1);

  EXPECT_CALL(*mock_highlighted_games_store_, SetPendingCallback(_)).Times(1);

  EXPECT_CALL(*mock_catalog_store_, ClearCache()).Times(1);

  games_service_->GetHighlightedGame(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // No-op.
      }));
}

}  // namespace games
