// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_iterator.h"
#include "components/query_tiles/internal/tile_manager.h"

namespace upboarding {
namespace {

class TileManagerImpl : public TileManager {
 public:
  TileManagerImpl(std::unique_ptr<TileStore> store, base::Clock* clock)
      : initialized_(false), store_(std::move(store)), clock_(clock) {}

  TileManagerImpl(const TileManagerImpl& other) = delete;
  TileManagerImpl& operator=(const TileManagerImpl& other) = delete;

 private:
  // TileManager implementation.
  void Init(TileGroupStatusCallback callback) override {
    store_->InitAndLoad(base::BindOnce(&TileManagerImpl::OnTileStoreInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
  }

  void SaveTiles(std::unique_ptr<TileGroup> group,
                 TileGroupStatusCallback callback) override {
    if (!initialized_) {
      std::move(callback).Run(TileGroupStatus::kUninitialized);
      return;
    }

    store_->Update(group->id, *group.get(),
                   base::BindOnce(&TileManagerImpl::OnGroupSaved,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(group), std::move(callback)));
  }

  void GetTiles(GetTilesCallback callback) override {
    std::vector<Tile> tiles;
    if (tile_group_ && ValidateGroup(tile_group_.get())) {
      for (const auto& tile : tile_group_->tiles)
        tiles.emplace_back(*tile.get());
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(tiles)));
  }

  void GetTile(const std::string& tile_id, TileCallback callback) override {
    const Tile* result = nullptr;
    if (tile_group_ && ValidateGroup(tile_group_.get())) {
      TileIterator it(*tile_group_, TileIterator::kAllTiles);
      while (it.HasNext()) {
        const auto* tile = it.Next();
        DCHECK(tile);
        if (tile->id == tile_id) {
          result = tile;
          break;
        }
      }
    }

    auto result_tile = result ? base::make_optional(*result) : base::nullopt;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result_tile));
  }

  void OnTileStoreInitialized(
      TileGroupStatusCallback callback,
      bool success,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    if (!success) {
      std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      return;
    }

    initialized_ = true;

    PruneInvalidGroup(std::move(callback), std::move(loaded_groups));
  }

  // Filters out and deletes invalid groups from db and memory.
  void PruneInvalidGroup(
      TileGroupStatusCallback callback,
      std::map<std::string, std::unique_ptr<TileGroup>> loaded_groups) {
    DCHECK_LE(loaded_groups.size(), 1u);

    TileGroupStatus status = TileGroupStatus::kSuccess;
    std::vector<std::string> to_deprecated_in_db;
    for (const auto& pair : loaded_groups) {
      auto group_id = pair.first;
      auto* group = pair.second.get();
      if (!ValidateGroup(group)) {
        status = TileGroupStatus::kInvalidGroup;
        to_deprecated_in_db.emplace_back(group_id);
      }
    }

    for (const auto& key : to_deprecated_in_db) {
      DeleteGroup(key);
      loaded_groups.erase(key);
    }

    // Moves the valid group into in memory holder.
    if (!loaded_groups.empty())
      std::swap(tile_group_, loaded_groups.begin()->second);
    std::move(callback).Run(status);
  }

  // Returns true if the group is not expired and the locale matches OS
  // setting.
  bool ValidateGroup(const TileGroup* group) const {
    return clock_->Now() - group->last_updated_ts <
           TileConfig::GetExpireDuration();
  }

  void OnGroupSaved(std::unique_ptr<TileGroup> group,
                    TileGroupStatusCallback callback,
                    bool success) {
    if (!success) {
      std::move(callback).Run(TileGroupStatus::kFailureDbOperation);
      return;
    }

    if (tile_group_)
      DeleteGroup(tile_group_->id);

    std::swap(tile_group_, group);
    std::move(callback).Run(TileGroupStatus::kSuccess);
  }

  void DeleteGroup(const std::string& key) {
    store_->Delete(key, base::BindOnce(&TileManagerImpl::OnGroupDeleted,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGroupDeleted(bool success) {
    // TODO(hesen): Record db operation metrics.
    NOTIMPLEMENTED();
  }

  // Indicates if the db is fully initialized, rejects calls if not.
  bool initialized_;

  // Storage layer of query tiles.
  std::unique_ptr<TileStore> store_;

  // The tile group in-memory holder.
  std::unique_ptr<TileGroup> tile_group_;

  // Clock object.
  base::Clock* clock_;

  base::WeakPtrFactory<TileManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

TileManager::TileManager() = default;

std::unique_ptr<TileManager> TileManager::Create(
    std::unique_ptr<TileStore> tile_store,
    base::Clock* clock) {
  return std::make_unique<TileManagerImpl>(std::move(tile_store), clock);
}

}  // namespace upboarding
