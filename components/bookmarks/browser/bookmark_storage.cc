// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_storage.h"

#include <stddef.h>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_codec.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_constants.h"

namespace bookmarks {

namespace {

// Extension used for backup files (copy of main file created during startup).
const base::FilePath::CharType kBackupExtension[] = FILE_PATH_LITERAL("bak");

// How often we save.
const int kSaveDelayMS = 2500;

void BackupCallback(const base::FilePath& path) {
  base::FilePath backup_path = path.ReplaceExtension(kBackupExtension);
  base::CopyFile(path, backup_path);
}

}  // namespace

// BookmarkStorage -------------------------------------------------------------

BookmarkStorage::BookmarkStorage(
    BookmarkModel* model,
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : model_(model),
      writer_(profile_path.Append(kBookmarksFileName),
              sequenced_task_runner,
              base::TimeDelta::FromMilliseconds(kSaveDelayMS),
              "BookmarkStorage"),
      sequenced_task_runner_(sequenced_task_runner) {}

BookmarkStorage::~BookmarkStorage() {
  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();
}

void BookmarkStorage::ScheduleSave() {
  switch (backup_state_) {
    case BACKUP_NONE:
      backup_state_ = BACKUP_DISPATCHED;
      sequenced_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&BackupCallback, writer_.path()),
          base::BindOnce(&BookmarkStorage::OnBackupFinished,
                         weak_factory_.GetWeakPtr()));
      return;
    case BACKUP_DISPATCHED:
      // Currently doing a backup which will call this function when done.
      return;
    case BACKUP_ATTEMPTED:
      writer_.ScheduleWrite(this);
      return;
  }
  NOTREACHED();
}

void BookmarkStorage::OnBackupFinished() {
  backup_state_ = BACKUP_ATTEMPTED;
  ScheduleSave();
}

void BookmarkStorage::BookmarkModelDeleted() {
  // We need to save now as otherwise by the time SaveNow is invoked
  // the model is gone.
  if (writer_.HasPendingWrite())
    SaveNow();
  model_ = nullptr;
}

bool BookmarkStorage::SerializeData(std::string* output) {
  BookmarkCodec codec;
  std::unique_ptr<base::Value> value(
      codec.Encode(model_, model_->client()->EncodeBookmarkSyncMetadata()));
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(value.get()));
}

bool BookmarkStorage::SaveNow() {
  if (!model_ || !model_->loaded()) {
    // We should only get here if we have a valid model and it's finished
    // loading.
    NOTREACHED();
    return false;
  }

  std::unique_ptr<std::string> data(new std::string);
  if (!SerializeData(data.get()))
    return false;
  writer_.WriteNow(std::move(data));
  return true;
}

}  // namespace bookmarks
