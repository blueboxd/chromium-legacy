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
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sessions/core/snapshotting_command_storage_backend.h"

namespace sessions {
namespace {

// Helper used by ScheduleGetLastSessionCommands. It runs callback on TaskRunner
// thread if it's not canceled.
void RunIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    SnapshottingCommandStorageManager::GetCommandsCallback callback,
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  if (is_canceled.Run())
    return;
  std::move(callback).Run(std::move(commands));
}

void PostOrRunInternalGetCommandsCallback(
    base::SequencedTaskRunner* task_runner,
    SnapshottingCommandStorageManager::GetCommandsCallback callback,
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  if (task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(std::move(commands));
  } else {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(commands)));
  }
}

}  // namespace

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
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);

  GetCommandsCallback run_if_not_canceled =
      base::BindOnce(&RunIfNotCanceled, is_canceled, std::move(callback));

  GetCommandsCallback callback_runner =
      base::BindOnce(&PostOrRunInternalGetCommandsCallback,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     std::move(run_if_not_canceled));

  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          &SnapshottingCommandStorageBackend::ReadLastSessionCommands,
          GetSnapshottingBackend(), is_canceled, std::move(callback_runner)));
  return id;
}

SnapshottingCommandStorageBackend*
SnapshottingCommandStorageManager::GetSnapshottingBackend() {
  return static_cast<SnapshottingCommandStorageBackend*>(backend());
}

}  // namespace sessions
