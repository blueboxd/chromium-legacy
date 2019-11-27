// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
#define COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_service.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/game.pb.h"
#include "components/prefs/pref_service.h"

namespace games {

class GamesServiceImpl : public GamesService {
 public:
  explicit GamesServiceImpl(PrefService* prefs);

  // Constructor to be used by unit tests.
  explicit GamesServiceImpl(std::unique_ptr<DataFilesParser> files_parser,
                            PrefService* prefs);
  ~GamesServiceImpl() override;

  void GetHighlightedGame(HighlightedGameCallback callback) override;

 private:
  void GetHighlightedGameFromCatalog(HighlightedGameCallback callback,
                                     std::unique_ptr<GamesCatalog> catalog);

  std::unique_ptr<GamesCatalog> GetCatalog();

  bool IsComponentInstalled();

  std::unique_ptr<DataFilesParser> files_parser_;

  // Will outlive the current instance.
  PrefService* prefs_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<const Game> cached_highlighted_game_;
  std::unique_ptr<const base::FilePath> cached_data_files_dir_;

  base::WeakPtrFactory<GamesServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GamesServiceImpl);
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
