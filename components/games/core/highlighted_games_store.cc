// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/highlighted_games_store.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/date.pb.h"

namespace games {

namespace {
bool TryConvertTime(const Date& date_proto, base::Time* out_time) {
  return base::Time::FromUTCExploded(
      {date_proto.year(), date_proto.month(), 0, date_proto.day()}, out_time);
}
}  // namespace

HighlightedGamesStore::HighlightedGamesStore(base::Clock* clock)
    : HighlightedGamesStore(std::make_unique<DataFilesParser>(), clock) {}

HighlightedGamesStore::HighlightedGamesStore(
    std::unique_ptr<DataFilesParser> data_files_parser,
    base::Clock* clock)
    : data_files_parser_(std::move(data_files_parser)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})),
      clock_(clock) {}

HighlightedGamesStore::~HighlightedGamesStore() = default;

void HighlightedGamesStore::ProcessAsync(const base::FilePath& install_dir,
                                         const GamesCatalog& catalog,
                                         base::OnceClosure done_callback) {
  // If cache is valid, we don't need to do extra processing.
  auto cached_game = TryGetFromCache();
  if (cached_game) {
    RespondAndInvoke(ResponseCode::kSuccess, cached_game.value(),
                     std::move(done_callback));
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&HighlightedGamesStore::GetHighlightedGamesResponse,
                     base::Unretained(this), install_dir),
      base::BindOnce(&HighlightedGamesStore::OnHighlightedGamesResponseParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     catalog));
}

bool HighlightedGamesStore::TryRespondFromCache() {
  if (!pending_callback_) {
    return false;
  }

  auto cached_game = TryGetFromCache();
  if (!cached_game) {
    return false;
  }

  Respond(ResponseCode::kSuccess, cached_game.value());
  return true;
}

base::Optional<Game> HighlightedGamesStore::TryGetFromCache() {
  base::Optional<Game> optional_game;

  if (!cached_highlighted_game_ || !cached_game_) {
    return optional_game;
  }

  if (IsCurrent(*cached_highlighted_game_)) {
    optional_game = *cached_game_;
  } else {
    // Current game is outdated, clear cache.
    cached_highlighted_game_.reset();
    cached_game_.reset();
  }

  return optional_game;
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

  base::Optional<HighlightedGamesResponse> response_proto =
      data_files_parser_->TryParseHighlightedGames(install_dir);
  if (!response_proto.has_value()) {
    return nullptr;
  }
  return std::make_unique<HighlightedGamesResponse>(response_proto.value());
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

  if (!response) {
    RespondAndInvoke(ResponseCode::kFileNotFound, Game(),
                     std::move(done_callback));
    return;
  }

  // Try to find the game of the day for today.
  for (const HighlightedGame& hg : response->games()) {
    if (IsCurrent(hg)) {
      // Try to update the cache with this game.
      base::Optional<Game> game = TryFindGameById(hg.game_id(), catalog);
      if (!game) {
        RespondAndInvoke(ResponseCode::kInvalidData, Game(),
                         std::move(done_callback));
        return;
      }

      cached_game_ = std::make_unique<Game>(game.value());
      cached_highlighted_game_ = std::make_unique<HighlightedGame>(hg);
      RespondAndInvoke(ResponseCode::kSuccess, *cached_game_,
                       std::move(done_callback));
      return;
    }
  }

  // Failed to find the game of the day.
  RespondAndInvoke(ResponseCode::kInvalidData, Game(),
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

bool HighlightedGamesStore::IsCurrent(const HighlightedGame& highlighted_game) {
  base::Time start_date;
  if (!TryConvertTime(highlighted_game.start_date(), &start_date)) {
    // TODO(crbug.com/1018201): Log bad data.
    return false;
  }

  base::Time end_date;
  if (!TryConvertTime(highlighted_game.end_date(), &end_date)) {
    // TODO(crbug.com/1018201): Log bad data.
    return false;
  }

  if (start_date > end_date) {
    // TODO(crbug.com/1018201): Log bad data.
    return false;
  }

  return clock_->Now() >= start_date && clock_->Now() < end_date;
}

}  // namespace games
