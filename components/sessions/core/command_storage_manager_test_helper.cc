// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager_test_helper.h"

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/snapshotting_command_storage_backend.h"

namespace sessions {
namespace {
bool IsCanceled() {
  return false;
}
}  // namespace

CommandStorageManagerTestHelper::CommandStorageManagerTestHelper(
    CommandStorageManager* command_storage_manager)
    : command_storage_manager_(command_storage_manager) {
  CHECK(command_storage_manager);
}

void CommandStorageManagerTestHelper::RunTaskOnBackendThread(
    const base::Location& from_here,
    base::OnceClosure task) {
  command_storage_manager_->backend_task_runner_->PostNonNestableTask(
      from_here, std::move(task));
}

bool CommandStorageManagerTestHelper::ProcessedAnyCommands() {
  return command_storage_manager_->backend_->inited() ||
         !command_storage_manager_->pending_commands().empty();
}

void CommandStorageManagerTestHelper::ReadLastSessionCommands(
    std::vector<std::unique_ptr<SessionCommand>>* commands) {
  static_cast<SnapshottingCommandStorageBackend*>(
      command_storage_manager_->backend_.get())
      ->ReadLastSessionCommands(
          base::BindRepeating(&IsCanceled),
          base::BindLambdaForTesting(
              [&commands](std::vector<std::unique_ptr<SessionCommand>> result) {
                *commands = std::move(result);
              }));
}

scoped_refptr<base::SequencedTaskRunner>
CommandStorageManagerTestHelper::GetBackendTaskRunner() {
  return command_storage_manager_->backend_task_runner_;
}

}  // namespace sessions
