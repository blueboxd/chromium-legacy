// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_MANAGER_H_
#define COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_MANAGER_H_

#include <memory>

#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/sessions_export.h"

namespace base {
class CancelableTaskTracker;
class FilePath;
}  // namespace base

namespace sessions {
class CommandStorageManagerDelegate;
class SnapshottingCommandStorageBackend;

// Adds snapshotting to CommandStorageManager. In this context a snapshot refers
// to a copy of the current session file (in code, the snapshot is referred to
// as the last session file). A snapshot is made at startup, but may also occur
// at other points. For example, when Chrome transitions from no tabbed browsers
// to having tabbed browsers a snapshot is made.
class SESSIONS_EXPORT SnapshottingCommandStorageManager
    : public CommandStorageManager {
 public:
  // Identifies the type of session service this is. This is used by the
  // backend to determine the name of the files.
  enum SessionType { SESSION_RESTORE, TAB_RESTORE };

  SnapshottingCommandStorageManager(SessionType type,
                                    const base::FilePath& path,
                                    CommandStorageManagerDelegate* delegate);

  SnapshottingCommandStorageManager(const SnapshottingCommandStorageManager&) =
      delete;
  SnapshottingCommandStorageManager& operator=(
      const SnapshottingCommandStorageManager&) = delete;

  ~SnapshottingCommandStorageManager() override;

  // Moves the current session to the last session.
  void MoveCurrentSessionToLastSession();

  // Deletes the last session.
  void DeleteLastSession();

  // Uses the backend to load the last session commands from disc. |callback|
  // gets called once the data has arrived.
  base::CancelableTaskTracker::TaskId ScheduleGetLastSessionCommands(
      GetCommandsCallback callback,
      base::CancelableTaskTracker* tracker);

 private:
  SnapshottingCommandStorageBackend* GetSnapshottingBackend();
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_MANAGER_H_
