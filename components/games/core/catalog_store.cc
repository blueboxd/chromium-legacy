// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/catalog_store.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"

namespace games {

CatalogStore::CatalogStore()
    : CatalogStore(std::make_unique<DataFilesParser>()) {}

CatalogStore::CatalogStore(std::unique_ptr<DataFilesParser> data_files_parser)
    : data_files_parser_(std::move(data_files_parser)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

CatalogStore::~CatalogStore() = default;

void CatalogStore::UpdateCatalogAsync(
    const base::FilePath& install_dir,
    base::OnceCallback<void(ResponseCode)> callback) {
  ClearCache();

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&CatalogStore::ParseCatalog, base::Unretained(this),
                     install_dir),
      base::BindOnce(&CatalogStore::OnCatalogParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CatalogStore::ClearCache() {
  cached_catalog_.reset();
}

std::unique_ptr<GamesCatalog> CatalogStore::ParseCatalog(
    const base::FilePath& install_dir) {
  // File IO should always be run using the thread pool task runner.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::Optional<GamesCatalog> catalog =
      data_files_parser_->TryParseCatalog(install_dir);

  return catalog.has_value() ? std::make_unique<GamesCatalog>(catalog.value())
                             : nullptr;
}

void CatalogStore::OnCatalogParsed(
    base::OnceCallback<void(ResponseCode)> callback,
    std::unique_ptr<GamesCatalog> catalog) {
  if (!catalog) {
    std::move(callback).Run(ResponseCode::kFileNotFound);
    return;
  }

  if (catalog->games().empty()) {
    std::move(callback).Run(ResponseCode::kInvalidData);
    return;
  }

  cached_catalog_ = std::move(catalog);
  std::move(callback).Run(ResponseCode::kSuccess);
}

}  // namespace games
