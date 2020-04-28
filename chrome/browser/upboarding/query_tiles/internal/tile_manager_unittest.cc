// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/upboarding/query_tiles/internal/config.h"
#include "chrome/browser/upboarding/query_tiles/internal/tile_store.h"
#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;

namespace upboarding {
namespace {

class MockTileStore : public Store<TileGroup> {
 public:
  MockTileStore() = default;
  MOCK_METHOD1(InitAndLoad, void(TileStore::LoadCallback));
  MOCK_METHOD3(Update,
               void(const std::string&, const TileGroup&, UpdateCallback));
  MOCK_METHOD2(Delete, void(const std::string&, TileStore::DeleteCallback));
};

class TileManagerTest : public testing::Test {
 public:
  TileManagerTest() = default;
  ~TileManagerTest() override = default;

  TileManagerTest(const TileManagerTest& other) = delete;
  TileManagerTest& operator=(const TileManagerTest& other) = delete;

  void SetUp() override {
    auto tile_store = std::make_unique<MockTileStore>();
    tile_store_ = tile_store.get();
    config_.locale = "en-US";
    config_.is_enabled = true;
    base::Time fake_now;
    EXPECT_TRUE(base::Time::FromString("03/18/20 01:00:00 AM", &fake_now));
    clock_.SetNow(fake_now);
    manager_ = TileManager::Create(std::move(tile_store), &clock_, &config_);
  }

  // Initial and load entries from store_, compare the |expected_status| to the
  // actual returned status.
  void Init(base::RepeatingClosure closure, TileGroupStatus expected_status) {
    manager()->Init(base::BindOnce(&TileManagerTest::OnInitCompleted,
                                   base::Unretained(this), std::move(closure),
                                   expected_status));
  }

  void OnInitCompleted(base::RepeatingClosure closure,
                       TileGroupStatus expected_status,
                       TileGroupStatus status) {
    EXPECT_EQ(status, expected_status);
    std::move(closure).Run();
  }

  // Run SaveTiles call from manager_, compare the |expected_status| to the
  // actual returned status.
  void SaveTiles(std::vector<std::unique_ptr<Tile>> tiles,
                 base::RepeatingClosure closure,
                 TileGroupStatus expected_status) {
    manager()->SaveTiles(
        std::move(tiles),
        base::BindOnce(&TileManagerTest::OnTilesSaved, base::Unretained(this),
                       std::move(closure), expected_status));
  }

  void OnTilesSaved(base::RepeatingClosure closure,
                    TileGroupStatus expected_status,
                    TileGroupStatus status) {
    EXPECT_EQ(status, expected_status);
    std::move(closure).Run();
  }

  // Run GetTiles call from manager_, compare the |expected| to the actual
  // returned tiles.
  void GetTiles(base::RepeatingClosure closure, std::vector<Tile> expected) {
    manager()->GetTiles(base::BindOnce(
        &TileManagerTest::OnTilesReturned, base::Unretained(this),
        std::move(closure), std::move(expected)));
  }

  void OnTilesReturned(base::RepeatingClosure closure,
                       std::vector<Tile> expected,
                       std::vector<Tile> tiles) {
    EXPECT_TRUE(test::AreTilesIdentical(expected, tiles));
    std::move(closure).Run();
  }

 protected:
  TileManager* manager() { return manager_.get(); }
  MockTileStore* tile_store() { return tile_store_; }
  const TileConfig& config() const { return config_; }
  const base::SimpleTestClock* clock() const { return &clock_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TileManager> manager_;
  MockTileStore* tile_store_;
  TileConfig config_;
  base::SimpleTestClock clock_;
};

// TODO(hesen): Add a test where we request tiles before the initialize
// callback from the DB.

TEST_F(TileManagerTest, InitAndLoadWithDbOperationFailed) {
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(
                              bool, MockTileStore::KeysAndEntries)> callback) {
        std::move(callback).Run(false, MockTileStore::KeysAndEntries());
      }));

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kFailureDbOperation);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);
  loop.Run();
}

TEST_F(TileManagerTest, InitWithEmptyDb) {
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(
                              bool, MockTileStore::KeysAndEntries)> callback) {
        std::move(callback).Run(true, MockTileStore::KeysAndEntries());
      }));

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kSuccess);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);
  loop.Run();
}

TEST_F(TileManagerTest, InitAndLoadWithLocaleNotMatch) {
  auto invalid_group = std::make_unique<TileGroup>();
  test::ResetTestGroup(invalid_group.get());
  invalid_group->locale = "jp";
  MockTileStore::KeysAndEntries input;
  input[invalid_group->id] = std::move(invalid_group);
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke(
          [&input](base::OnceCallback<void(bool, MockTileStore::KeysAndEntries)>
                       callback) {
            std::move(callback).Run(true, std::move(input));
          }));

  EXPECT_CALL(*tile_store(), Delete(_, _));

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kInvalidGroup);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);
  loop.Run();
}

TEST_F(TileManagerTest, InitAndLoadWithExpiredGroup) {
  auto invalid_group = std::make_unique<TileGroup>();
  test::ResetTestGroup(invalid_group.get());
  invalid_group->last_updated_ts =
      clock()->Now() - base::TimeDelta::FromDays(3);
  MockTileStore::KeysAndEntries input;
  input[invalid_group->id] = std::move(invalid_group);
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke(
          [&input](base::OnceCallback<void(bool, MockTileStore::KeysAndEntries)>
                       callback) {
            std::move(callback).Run(true, std::move(input));
          }));

  EXPECT_CALL(*tile_store(), Delete(_, _));

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kInvalidGroup);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);
  loop.Run();
}

TEST_F(TileManagerTest, InitAndLoadSuccess) {
  auto input_group = std::make_unique<TileGroup>();
  test::ResetTestGroup(input_group.get());
  std::vector<Tile> expected;
  input_group->last_updated_ts =
      clock()->Now() - base::TimeDelta::FromMinutes(5);
  for (const auto& tile : input_group->tiles)
    expected.emplace_back(*tile.get());

  MockTileStore::KeysAndEntries input;
  input[input_group->id] = std::move(input_group);
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke(
          [&input](base::OnceCallback<void(bool, MockTileStore::KeysAndEntries)>
                       callback) {
            std::move(callback).Run(true, std::move(input));
          }));

  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kSuccess);
  GetTiles(loop.QuitClosure(), expected);
  loop.Run();
}

// Failed to init an empty db, and save tiles call failed because of db is
// uninitialized. GetTiles should return empty result.
TEST_F(TileManagerTest, SaveTilesWhenUnintialized) {
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(
                              bool, MockTileStore::KeysAndEntries)> callback) {
        std::move(callback).Run(false, MockTileStore::KeysAndEntries());
      }));
  EXPECT_CALL(*tile_store(), Update(_, _, _)).Times(0);
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kFailureDbOperation);

  auto tile_to_save = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));

  SaveTiles(std::move(tiles_to_save), loop.QuitClosure(),
            TileGroupStatus::kUninitialized);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);

  loop.Run();
}

// Init with empty db successfully, and save tiles failed because of db
// operation failed. GetTiles should return empty result.
TEST_F(TileManagerTest, SaveTilesFailed) {
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(
                              bool, MockTileStore::KeysAndEntries)> callback) {
        std::move(callback).Run(true, MockTileStore::KeysAndEntries());
      }));
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(false);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kSuccess);

  auto tile_to_save = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));

  SaveTiles(std::move(tiles_to_save), loop.QuitClosure(),
            TileGroupStatus::kFailureDbOperation);
  GetTiles(loop.QuitClosure(), std::vector<Tile>() /*expect an empty result*/);

  loop.Run();
}

// Init with empty db successfully, and save tiles successfully. GetTiles should
// return the recent saved tiles, and no Delete call is executed.
TEST_F(TileManagerTest, SaveTilesSuccess) {
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(
                              bool, MockTileStore::KeysAndEntries)> callback) {
        std::move(callback).Run(true, MockTileStore::KeysAndEntries());
      }));
  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(*tile_store(), Delete(_, _)).Times(0);

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kSuccess);

  auto tile_to_save = std::make_unique<Tile>();
  auto expected_tile = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  test::ResetTestEntry(expected_tile.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));
  std::vector<Tile> expected;
  expected.emplace_back(*expected_tile.get());

  SaveTiles(std::move(tiles_to_save), loop.QuitClosure(),
            TileGroupStatus::kSuccess);
  GetTiles(loop.QuitClosure(), std::move(expected));
  loop.Run();
}

// Init with store successfully. The store originally has entries loaded into
// memory. Save new tiles successfully. GetTiles should return the recent saved
// tiles, and Delete() call is executed in store, also replace the old group in
// memory.
TEST_F(TileManagerTest, SaveTilesAndReplaceOldGroupSuccess) {
  auto input_group = std::make_unique<TileGroup>();
  test::ResetTestGroup(input_group.get());
  input_group->last_updated_ts =
      clock()->Now() - base::TimeDelta::FromMinutes(5);

  MockTileStore::KeysAndEntries input;
  input[input_group->id] = std::move(input_group);
  EXPECT_CALL(*tile_store(), InitAndLoad(_))
      .WillOnce(Invoke(
          [&input](base::OnceCallback<void(bool, MockTileStore::KeysAndEntries)>
                       callback) {
            std::move(callback).Run(true, std::move(input));
          }));

  EXPECT_CALL(*tile_store(), Update(_, _, _))
      .WillOnce(Invoke([](const std::string& id, const TileGroup& group,
                          MockTileStore::UpdateCallback callback) {
        std::move(callback).Run(true);
      }));

  EXPECT_CALL(*tile_store(), Delete("group_guid", _));

  base::RunLoop loop;
  Init(loop.QuitClosure(), TileGroupStatus::kSuccess);

  auto tile_to_save = std::make_unique<Tile>();
  test::ResetTestEntry(tile_to_save.get());
  std::vector<std::unique_ptr<Tile>> tiles_to_save;
  tiles_to_save.emplace_back(std::move(tile_to_save));

  auto expected_tile = std::make_unique<Tile>();
  test::ResetTestEntry(expected_tile.get());
  std::vector<Tile> expected;
  expected.emplace_back(std::move(*expected_tile.get()));

  SaveTiles(std::move(tiles_to_save), loop.QuitClosure(),
            TileGroupStatus::kSuccess);
  GetTiles(loop.QuitClosure(), std::move(expected));
  loop.Run();
}

}  // namespace

}  // namespace upboarding
