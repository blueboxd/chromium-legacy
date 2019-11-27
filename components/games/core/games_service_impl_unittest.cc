// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/games/core/games_service_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/test/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace games {

namespace {

class MockDataFilesParser : public DataFilesParser {
 public:
  ~MockDataFilesParser() override {}

  MOCK_METHOD2(TryParseCatalog, bool(const base::FilePath&, GamesCatalog*));
};

}  // namespace

class GamesServiceImplTest : public testing::Test {
 protected:
  void SetUp() override {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    games::prefs::RegisterProfilePrefs(test_pref_service_->registry());

    auto mock_data_files_parser = std::make_unique<MockDataFilesParser>();
    mock_data_files_parser_ = mock_data_files_parser.get();

    games_service_ = std::make_unique<GamesServiceImpl>(
        std::move(mock_data_files_parser), test_pref_service_.get());
  }

  void SetInstallDirPref() {
    prefs::SetInstallDirPath(test_pref_service_.get(), fake_install_dir_);
  }

  void AssertGetHighlightedGameFailsWith(ResponseCode expected_code) {
    bool callback_called = false;

    base::RunLoop run_loop;
    games_service_->GetHighlightedGame(base::BindLambdaForTesting(
        [&](ResponseCode given_code, const Game* given_game) {
          EXPECT_EQ(expected_code, given_code);
          callback_called = true;
          run_loop.Quit();
        }));

    run_loop.Run();
    EXPECT_TRUE(callback_called);
  }

  // TaskEnvironment is used instead of SingleThreadTaskEnvironment since we
  // post a task to the thread pool.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockDataFilesParser* mock_data_files_parser_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<GamesServiceImpl> games_service_;
  base::FilePath fake_install_dir_ =
      base::FilePath(FILE_PATH_LITERAL("some/path"));
};

TEST_F(GamesServiceImplTest, GetHighlightedGame_ReturnsFirstFromCatalog) {
  SetInstallDirPref();

  // Create two games with different IDs, to make sure we can validate which one
  // was picked.
  GamesCatalog fake_catalog = test::CreateGamesCatalog(
      {test::CreateGame(/*id=*/1), test::CreateGame(/*id=*/2)});
  const Game& fake_highlighted_game = fake_catalog.games(0);

  EXPECT_CALL(*mock_data_files_parser_, TryParseCatalog(fake_install_dir_, _))
      .WillOnce([&fake_catalog](const base::FilePath& install_dir,
                                GamesCatalog* out_catalog) {
        *out_catalog = fake_catalog;
        return true;
      });

  const Game* result_game;

  base::RunLoop run_loop;
  games_service_->GetHighlightedGame(base::BindLambdaForTesting(
      [&](ResponseCode code, const Game* given_game) {
        ASSERT_EQ(ResponseCode::kSuccess, code);
        result_game = given_game;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_NE(nullptr, result_game);
  EXPECT_TRUE(test::AreProtosEqual(fake_highlighted_game, *result_game));
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_CachesHighlightedGame) {
  SetInstallDirPref();

  GamesCatalog fake_catalog = test::CreateGamesCatalogWithOneGame();
  const Game& fake_highlighted_game = fake_catalog.games(0);

  // Since caching is supposed to be working, we only expect the catalog to be
  // retrieved once.
  EXPECT_CALL(*mock_data_files_parser_, TryParseCatalog(fake_install_dir_, _))
      .Times(1)
      .WillOnce([&fake_catalog](const base::FilePath& install_dir,
                                GamesCatalog* out_catalog) {
        *out_catalog = fake_catalog;
        return true;
      });

  // Call GetHighlightedGame twice.
  for (int i = 0; i < 2; i++) {
    const Game* result_game;
    base::RunLoop run_loop;
    games_service_->GetHighlightedGame(base::BindLambdaForTesting(
        [&](ResponseCode code, const Game* given_game) {
          EXPECT_EQ(ResponseCode::kSuccess, code);
          result_game = given_game;
          run_loop.Quit();
        }));

    run_loop.Run();

    EXPECT_NE(nullptr, result_game);
    EXPECT_TRUE(test::AreProtosEqual(fake_highlighted_game, *result_game));
  }
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_ComponentNotInstalled) {
  AssertGetHighlightedGameFailsWith(ResponseCode::kFileNotFound);
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_CatalogNotFound) {
  SetInstallDirPref();

  EXPECT_CALL(*mock_data_files_parser_, TryParseCatalog(fake_install_dir_, _))
      .WillOnce([](const base::FilePath& install_dir,
                   GamesCatalog* out_catalog) { return false; });

  AssertGetHighlightedGameFailsWith(ResponseCode::kFileNotFound);
}

TEST_F(GamesServiceImplTest, GetHighlightedGame_CatalogEmpty) {
  SetInstallDirPref();

  GamesCatalog empty_catalog;

  EXPECT_CALL(*mock_data_files_parser_, TryParseCatalog(fake_install_dir_, _))
      .WillOnce([&empty_catalog](const base::FilePath& install_dir,
                                 GamesCatalog* out_catalog) {
        *out_catalog = empty_catalog;
        return true;
      });

  AssertGetHighlightedGameFailsWith(ResponseCode::kInvalidData);
}

}  // namespace games
