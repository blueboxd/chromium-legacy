// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/snapshotting_command_storage_backend.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/sessions/core/session_constants.h"

namespace sessions {
namespace {

base::FilePath GetCurrentFilePath(
    SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& base_path) {
  base::FilePath path = base_path;
  if (type == SnapshottingCommandStorageManager::TAB_RESTORE)
    path = path.Append(kCurrentTabSessionFileName);
  else
    path = path.Append(kCurrentSessionFileName);
  return path;
}

base::FilePath GetLastFilePath(
    SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& base_path) {
  base::FilePath path = base_path;
  if (type == SnapshottingCommandStorageManager::TAB_RESTORE)
    path = path.Append(kLastTabSessionFileName);
  else
    path = path.Append(kLastSessionFileName);
  return path;
}

}  // namespace

SnapshottingCommandStorageBackend::SnapshottingCommandStorageBackend(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
    SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& path_to_dir)
    : CommandStorageBackend(std::move(owning_task_runner),
                            GetCurrentFilePath(type, path_to_dir)),
      last_file_path_(GetLastFilePath(type, path_to_dir)) {
  // NOTE: this is invoked on the main thread, don't do file access here.
}

void SnapshottingCommandStorageBackend::ReadLastSessionCommands(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    GetCommandsCallback callback) {
  if (is_canceled.Run())
    return;

  InitIfNecessary();

  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  ReadCommandsFromFile(last_file_path_, std::vector<uint8_t>(), &commands);
  std::move(callback).Run(std::move(commands));
}

void SnapshottingCommandStorageBackend::DeleteLastSession() {
  InitIfNecessary();
  base::DeleteFile(last_file_path_, false);
}

void SnapshottingCommandStorageBackend::MoveCurrentSessionToLastSession() {
  InitIfNecessary();
  CloseFile();

  if (base::PathExists(last_file_path_))
    base::DeleteFile(last_file_path_, false);
  if (base::PathExists(path()))
    last_session_valid_ = base::Move(path(), last_file_path_);

  if (base::PathExists(path()))
    base::DeleteFile(path(), false);

  // Create and open the file for the current session.
  TruncateFile();
}

void SnapshottingCommandStorageBackend::DoInit() {
  MoveCurrentSessionToLastSession();
}

SnapshottingCommandStorageBackend::~SnapshottingCommandStorageBackend() =
    default;

}  // namespace sessions
