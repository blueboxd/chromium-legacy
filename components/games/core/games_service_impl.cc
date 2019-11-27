// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/proto/game.pb.h"

namespace games {

GamesServiceImpl::GamesServiceImpl(PrefService* prefs)
    : GamesServiceImpl(std::make_unique<DataFilesParser>(), prefs) {}

GamesServiceImpl::GamesServiceImpl(
    std::unique_ptr<DataFilesParser> files_parser,
    PrefService* prefs)
    : files_parser_(std::move(files_parser)),
      prefs_(prefs),
      task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})) {
}

GamesServiceImpl::~GamesServiceImpl() = default;

void GamesServiceImpl::GetHighlightedGame(HighlightedGameCallback callback) {
  if (cached_highlighted_game_) {
    std::move(callback).Run(ResponseCode::kSuccess,
                            cached_highlighted_game_.get());
    return;
  }

  // If the Games component wasn't downloaded, we cannot provide the surface
  // with a highlighted game.
  if (!IsComponentInstalled()) {
    std::move(callback).Run(ResponseCode::kFileNotFound, nullptr);
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&GamesServiceImpl::GetCatalog, base::Unretained(this)),
      base::BindOnce(&GamesServiceImpl::GetHighlightedGameFromCatalog,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<GamesCatalog> GamesServiceImpl::GetCatalog() {
  // File IO should always be run using the thread pool task runner.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto catalog = std::make_unique<GamesCatalog>();
  if (!files_parser_->TryParseCatalog(*cached_data_files_dir_, catalog.get())) {
    return nullptr;
  }

  return catalog;
}

void GamesServiceImpl::GetHighlightedGameFromCatalog(
    HighlightedGameCallback callback,
    std::unique_ptr<GamesCatalog> catalog) {
  if (!catalog) {
    std::move(callback).Run(ResponseCode::kFileNotFound, nullptr);
    return;
  }

  if (catalog->games().empty()) {
    std::move(callback).Run(ResponseCode::kInvalidData, nullptr);
    return;
  }

  // Let's use the first game in the catalog as our highlighted game.
  cached_highlighted_game_ = std::make_unique<const Game>(catalog->games(0));
  std::move(callback).Run(ResponseCode::kSuccess,
                          cached_highlighted_game_.get());
}

bool GamesServiceImpl::IsComponentInstalled() {
  if (cached_highlighted_game_ && !cached_data_files_dir_->empty()) {
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

}  // namespace games
