// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/snapshotting_command_storage_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/sessions/core/snapshotting_command_storage_backend.h"

namespace sessions {

SnapshottingCommandStorageManager::SnapshottingCommandStorageManager(
    SessionType type,
    const base::FilePath& path,
    CommandStorageManagerDelegate* delegate)
    : CommandStorageManager(
          base::MakeRefCounted<SnapshottingCommandStorageBackend>(
              CreateDefaultBackendTaskRunner(),
              type,
              path),
          delegate) {}

SnapshottingCommandStorageManager::~SnapshottingCommandStorageManager() =
    default;

void SnapshottingCommandStorageManager::MoveCurrentSessionToLastSession() {
  Save();
  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &SnapshottingCommandStorageBackend::MoveCurrentSessionToLastSession,
          GetSnapshottingBackend()));
}

void SnapshottingCommandStorageManager::DeleteLastSession() {
  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&SnapshottingCommandStorageBackend::DeleteLastSession,
                     GetSnapshottingBackend()));
}

base::CancelableTaskTracker::TaskId
SnapshottingCommandStorageManager::ScheduleGetLastSessionCommands(
    GetCommandsCallback callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  GetCommandsCallback backend_callback;
  const base::CancelableTaskTracker::TaskId id = CreateCallbackForGetCommands(
      tracker, std::move(callback), &is_canceled, &backend_callback);

  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &SnapshottingCommandStorageBackend::ReadLastSessionCommands,
          GetSnapshottingBackend(), is_canceled, std::move(backend_callback)));
  return id;
}

SnapshottingCommandStorageBackend*
SnapshottingCommandStorageManager::GetSnapshottingBackend() {
  return static_cast<SnapshottingCommandStorageBackend*>(backend());
}

}  // namespace sessions
