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
#include "base/run_loop.h"
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

  void SetHighlightedGamesStoreCacheWith(const Game& game) {
    EXPECT_CALL(*mock_highlighted_games_store_, TryGetFromCache())
        .WillOnce([&game]() { return base::Optional<Game>(game); });
  }

  void SetHighlightedGamesStoreCacheEmpty() {
    EXPECT_CALL(*mock_highlighted_games_store_, TryGetFromCache())
        .WillOnce([]() { return base::nullopt; });
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
  base::RunLoop run_loop;
  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&run_loop](ResponseCode code, const Game game) {
        EXPECT_EQ(ResponseCode::kFileNotFound, code);
        EXPECT_TRUE(test::AreProtosEqual(game, Game()));
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_RetrievesFromCache) {
  // Mock component to be installed.
  SetInstallDirPref();

  Game fake_game = test::CreateGame();
  SetHighlightedGamesStoreCacheWith(fake_game);

  base::RunLoop run_loop;
  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&fake_game, &run_loop](ResponseCode code, const Game game) {
        EXPECT_EQ(ResponseCode::kSuccess, code);
        EXPECT_TRUE(test::AreProtosEqual(game, fake_game));
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_Success) {
  SetInstallDirPref();
  SetHighlightedGamesStoreCacheEmpty();

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
        ASSERT_TRUE(test::AreProtosEqual(fake_catalog, catalog));

        // Invoke the done callback to signal that the HighlightedStore is done
        // processing.
        std::move(done_callback).Run();
      });

  base::RunLoop run_loop;
  // Upon full success, the cached catalog will get deleted.
  EXPECT_CALL(*mock_catalog_store_, ClearCache()).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  games_service_->GetHighlightedGame(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // No-op.
      }));

  run_loop.Run();

  EXPECT_FALSE(games_service_->is_updating());
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_CatalogFileNotFound) {
  SetInstallDirPref();
  SetHighlightedGamesStoreCacheEmpty();

  EXPECT_CALL(*mock_catalog_store_, UpdateCatalogAsync(fake_install_dir_, _))
      .WillOnce([](const base::FilePath& install_dir,
                   base::OnceCallback<void(ResponseCode)> callback) {
        std::move(callback).Run(ResponseCode::kFileNotFound);
      });

  EXPECT_CALL(*mock_highlighted_games_store_,
              HandleCatalogFailure(ResponseCode::kFileNotFound))
      .Times(1);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_catalog_store_, ClearCache()).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  games_service_->GetHighlightedGame(
      base::BindLambdaForTesting([](ResponseCode code, const Game game) {
        // No-op.
      }));

  run_loop.Run();
}

}  // namespace games
