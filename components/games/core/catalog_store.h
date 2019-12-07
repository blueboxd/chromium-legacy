// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_CATALOG_STORE_H_
#define COMPONENTS_GAMES_CORE_CATALOG_STORE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/games_catalog.pb.h"

namespace games {

// This data store is in charge of parsing and caching the GamesCatalog proto
// that is downloaded on the client via the Games Data Files Omaha component.
class CatalogStore {
 public:
  explicit CatalogStore();

  // For unit tests.
  explicit CatalogStore(std::unique_ptr<DataFilesParser> data_files_parser);

  virtual ~CatalogStore();

  // Parses the catalog located under the given installation directory, then
  // caches the catalog instance. Will invoke the callback with the ResponseCode
  // representing the outcome from parsing the catalog.
  virtual void UpdateCatalogAsync(
      const base::FilePath& install_dir,
      base::OnceCallback<void(ResponseCode)> callback);

  // Deletes the cached catalog.
  virtual void ClearCache();

  const GamesCatalog* cached_catalog() const { return cached_catalog_.get(); }

 protected:
  std::unique_ptr<GamesCatalog> ParseCatalog(const base::FilePath& install_dir);

  void OnCatalogParsed(base::OnceCallback<void(ResponseCode)> callback,
                       std::unique_ptr<GamesCatalog> catalog);

  std::unique_ptr<DataFilesParser> data_files_parser_;

  // Task runner delegating tasks to the ThreadPool.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<GamesCatalog> cached_catalog_;

  base::WeakPtrFactory<CatalogStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CatalogStore);
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_CATALOG_STORE_H_
