// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager_delegate.h"

namespace sessions {

// Delay between when a command is received, and when we save it to the
// backend.
constexpr base::TimeDelta kSaveDelay = base::TimeDelta::FromMilliseconds(2500);

CommandStorageManager::CommandStorageManager(
    const base::FilePath& path,
    CommandStorageManagerDelegate* delegate)
    : CommandStorageManager(base::MakeRefCounted<CommandStorageBackend>(
                                CreateDefaultBackendTaskRunner(),
                                path),
                            delegate) {}

CommandStorageManager::~CommandStorageManager() = default;

void CommandStorageManager::ScheduleCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(std::move(command));
  StartSaveTimer();
}

void CommandStorageManager::AppendRebuildCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  pending_commands_.push_back(std::move(command));
}

void CommandStorageManager::EraseCommand(SessionCommand* old_command) {
  auto it = std::find_if(
      pending_commands_.begin(), pending_commands_.end(),
      [old_command](const std::unique_ptr<SessionCommand>& command_ptr) {
        return command_ptr.get() == old_command;
      });
  CHECK(it != pending_commands_.end());
  pending_commands_.erase(it);
}

void CommandStorageManager::SwapCommand(
    SessionCommand* old_command,
    std::unique_ptr<SessionCommand> new_command) {
  auto it = std::find_if(
      pending_commands_.begin(), pending_commands_.end(),
      [old_command](const std::unique_ptr<SessionCommand>& command_ptr) {
        return command_ptr.get() == old_command;
      });
  CHECK(it != pending_commands_.end());
  *it = std::move(new_command);
}

void CommandStorageManager::ClearPendingCommands() {
  pending_commands_.clear();
}

void CommandStorageManager::StartSaveTimer() {
  // Don't start a timer when testing.
  if (delegate_->ShouldUseDelayedSave() &&
      base::ThreadTaskRunnerHandle::IsSet() && !weak_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CommandStorageManager::Save,
                       weak_factory_.GetWeakPtr()),
        kSaveDelay);
  }
}

void CommandStorageManager::Save() {
  // Inform the delegate that we will save the commands now, giving it the
  // opportunity to append more commands.
  delegate_->OnWillSaveCommands();

  if (pending_commands_.empty())
    return;

  // We create a new vector which will receive all elements from the
  // current commands. This will also clear the current list.
  backend_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&CommandStorageBackend::AppendCommands, backend_,
                     std::move(pending_commands_), pending_reset_));

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

CommandStorageManager::CommandStorageManager(
    scoped_refptr<CommandStorageBackend> backend,
    CommandStorageManagerDelegate* delegate)
    : backend_(std::move(backend)),
      delegate_(delegate),
      backend_task_runner_(backend_->owning_task_runner()) {}

// static
scoped_refptr<base::SequencedTaskRunner>
CommandStorageManager::CreateDefaultBackendTaskRunner() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace sessions
