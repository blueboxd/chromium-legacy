// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_TEST_MOCKS_H_
#define COMPONENTS_GAMES_CORE_TEST_MOCKS_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/games/core/catalog_store.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_types.h"
#include "components/games/core/highlighted_games_store.h"
#include "components/games/core/proto/game.pb.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace games {
namespace test {

class MockDataFilesParser : public DataFilesParser {
 public:
  explicit MockDataFilesParser();
  ~MockDataFilesParser() override;

  MOCK_METHOD1(TryParseCatalog,
               base::Optional<GamesCatalog>(const base::FilePath&));
  MOCK_METHOD1(TryParseHighlightedGames,
               base::Optional<HighlightedGamesResponse>(const base::FilePath&));
};

class MockCatalogStore : public CatalogStore {
 public:
  explicit MockCatalogStore();
  ~MockCatalogStore() override;

  MOCK_METHOD2(UpdateCatalogAsync,
               void(const base::FilePath&,
                    base::OnceCallback<void(ResponseCode)>));
  MOCK_METHOD0(ClearCache, void());

  void set_cached_catalog(const GamesCatalog* catalog);
};

class MockHighlightedGamesStore : public HighlightedGamesStore {
 public:
  explicit MockHighlightedGamesStore();
  ~MockHighlightedGamesStore() override;

  MOCK_METHOD3(ProcessAsync,
               void(const base::FilePath&,
                    const GamesCatalog&,
                    base::OnceClosure));
  MOCK_METHOD0(TryGetFromCache, base::Optional<Game>());
  MOCK_METHOD0(TryRespondFromCache, bool());
  MOCK_METHOD1(SetPendingCallback, void(HighlightedGameCallback));
  MOCK_METHOD1(HandleCatalogFailure, void(ResponseCode));
};

class MockClock : public base::Clock {
 public:
  explicit MockClock();
  ~MockClock() override;

  void MockNow(const base::Time& fake_time);

  base::Time Now() const override;

  void AdvanceDays(int days);

 protected:
  base::Time mock_now_;
};

}  // namespace test
}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_TEST_MOCKS_H_
