// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/highlighted_games_store.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"

namespace games {

HighlightedGamesStore::HighlightedGamesStore()
    : HighlightedGamesStore(std::make_unique<DataFilesParser>()) {}

HighlightedGamesStore::HighlightedGamesStore(
    std::unique_ptr<DataFilesParser> data_files_parser)
    : data_files_parser_(std::move(data_files_parser)),
      task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})) {
}

HighlightedGamesStore::~HighlightedGamesStore() = default;

void HighlightedGamesStore::ProcessAsync(const base::FilePath& install_dir,
                                         const GamesCatalog& catalog,
                                         base::OnceClosure done_callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&HighlightedGamesStore::GetHighlightedGamesResponse,
                     base::Unretained(this), install_dir),
      base::BindOnce(&HighlightedGamesStore::OnHighlightedGamesResponseParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     catalog));
}

base::Optional<Game> HighlightedGamesStore::TryGetFromCache() {
  base::Optional<Game> cached_game;
  if (cached_highlighted_game_) {
    cached_game = *cached_highlighted_game_;
  }
  return cached_game;
}

void HighlightedGamesStore::SetPendingCallback(
    HighlightedGameCallback callback) {
  DCHECK(!pending_callback_);
  pending_callback_ = std::move(callback);
}

void HighlightedGamesStore::HandleCatalogFailure(ResponseCode failure_code) {
  Respond(failure_code, Game());
}

std::unique_ptr<HighlightedGamesResponse>
HighlightedGamesStore::GetHighlightedGamesResponse(
    const base::FilePath& install_dir) {
  // Must run file IO on the thread pool.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1018201): Add data file parsing logic.
  return std::make_unique<HighlightedGamesResponse>();
}

void HighlightedGamesStore::OnHighlightedGamesResponseParsed(
    base::OnceClosure done_callback,
    const GamesCatalog& catalog,
    std::unique_ptr<HighlightedGamesResponse> response) {
  if (catalog.games().empty()) {
    RespondAndInvoke(ResponseCode::kInvalidData, Game(),
                     std::move(done_callback));
    return;
  }

  // TODO(crbug.com/1018201): Add highlighted game parsing logic. For now, we'll
  // just return the first game from the catalog.
  cached_highlighted_game_ = std::make_unique<const Game>(catalog.games(0));
  RespondAndInvoke(ResponseCode::kSuccess, *cached_highlighted_game_,
                   std::move(done_callback));
}

void HighlightedGamesStore::Respond(ResponseCode code, const Game& game) {
  if (pending_callback_) {
    std::move(pending_callback_.value()).Run(code, game);
    pending_callback_ = base::nullopt;
  }
}

void HighlightedGamesStore::RespondAndInvoke(ResponseCode code,
                                             const Game& game,
                                             base::OnceClosure done_callback) {
  Respond(code, game);
  std::move(done_callback).Run();
}

}  // namespace games
