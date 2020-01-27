// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/proto/game.pb.h"

namespace games {

namespace {

// Used by the barrier closure to know when all of the feature stores are done
// processing the catalog. Increment when implementing new feature stores.
constexpr int kNumberOfFeatureStores = 1;

}  // namespace

GamesServiceImpl::GamesServiceImpl(
    std::unique_ptr<CatalogStore> catalog_store,
    std::unique_ptr<HighlightedGamesStore> highlighted_games_store,
    PrefService* prefs)
    : catalog_store_(std::move(catalog_store)),
      highlighted_games_store_(std::move(highlighted_games_store)),
      prefs_(prefs) {}

GamesServiceImpl::~GamesServiceImpl() = default;

void GamesServiceImpl::SetHighlightedGameCallback(
    HighlightedGameCallback callback) {
  highlighted_games_store_->SetPendingCallback(std::move(callback));
}

void GamesServiceImpl::GenerateHub() {
  if (!IsComponentInstalled()) {
    HandleFailure(ResponseCode::kComponentNotInstalled);
    return;
  }

  // Try to respond from cache to have the highlighted game appear faster.
  if (highlighted_games_store_->TryRespondFromCache()) {
    // TODO(crbug.com/1018201): Remove return when we have other stores that
    // don't have caching support; this is a temporary optimization.
    return;
  }

  UpdateStores();
}

void GamesServiceImpl::UpdateStores() {
  if (is_updating()) {
    // Ignore subsequent calls while already updating.
    return;
  }

  is_updating_ = true;

  catalog_store_->UpdateCatalogAsync(
      *cached_data_files_dir_,
      base::BindOnce(&GamesServiceImpl::OnCatalogReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GamesServiceImpl::OnCatalogReceived(ResponseCode code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (code == ResponseCode::kSuccess && !catalog_store_->cached_catalog()) {
    code = ResponseCode::kMissingCatalog;
  }

  if (code != ResponseCode::kSuccess) {
    // Make sure all feature stores handle the failure such that pending
    // callbacks are invoked (letting the UI know something failed).
    HandleFailure(code);
    return;
  }

  barrier_closure_ = base::BarrierClosure(
      kNumberOfFeatureStores, base::BindOnce(&GamesServiceImpl::DoneUpdating,
                                             weak_ptr_factory_.GetWeakPtr()));

  // Invoke all feature stores to process the parsed catalog and update their
  // caches. When adding a new store, we must increment kNumberOfFeatureStores.
  highlighted_games_store_->ProcessAsync(*cached_data_files_dir_,
                                         *catalog_store_->cached_catalog(),
                                         barrier_closure_);
}

void GamesServiceImpl::DoneUpdating() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset state.
  catalog_store_->ClearCache();
  is_updating_ = false;
}

bool GamesServiceImpl::IsComponentInstalled() {
  if (cached_data_files_dir_ && !cached_data_files_dir_->empty()) {
    return true;
  }

  base::FilePath install_dir;
  if (!prefs::TryGetInstallDirPath(prefs_, &install_dir)) {
    return false;
  }

  cached_data_files_dir_ =
      std::make_unique<base::FilePath>(std::move(install_dir));
  return true;
}

void GamesServiceImpl::HandleFailure(ResponseCode code) {
  highlighted_games_store_->HandleCatalogFailure(code);

  if (is_updating()) {
    DoneUpdating();
  }
}

}  // namespace games
