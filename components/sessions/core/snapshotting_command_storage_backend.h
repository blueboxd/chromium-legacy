// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_BACKEND_H_
#define COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_BACKEND_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/snapshotting_command_storage_manager.h"

namespace base {
class FilePath;
}

namespace sessions {

// Adds the ability to snapshot the current session file. The snapshot is
// referred to as 'last'
class SESSIONS_EXPORT SnapshottingCommandStorageBackend
    : public CommandStorageBackend {
 public:
  // Creates a CommandStorageBackend. This method is invoked on the MAIN thread,
  // and does no IO. The real work is done from Init, which is invoked on
  // a background task runer.
  //
  // |path_to_dir| gives the path the files are written two, and |type|
  // indicates which service is using this backend. |type| is used to determine
  // the name of the files to use as well as for logging.
  SnapshottingCommandStorageBackend(
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
      SnapshottingCommandStorageManager::SessionType type,
      const base::FilePath& path_to_dir);

  SnapshottingCommandStorageBackend(const SnapshottingCommandStorageBackend&) =
      delete;
  SnapshottingCommandStorageBackend& operator=(
      const SnapshottingCommandStorageBackend&) = delete;

  // Parses out the timestamp from a path pointing to a session file.
  static bool TimestampFromPath(const base::FilePath& path, base::Time& result);

  // Generates the path to a session file with the given timestamp.
  static base::FilePath FilePathFromTime(
      const SnapshottingCommandStorageManager::SessionType type,
      const base::FilePath& base_path,
      base::Time time);

  // Returns the commands from the last session file.
  std::vector<std::unique_ptr<SessionCommand>> ReadLastSessionCommands();

  // Deletes the file containing the commands for the last session.
  void DeleteLastSession();

  // Moves the current session file to the last session file. This is typically
  // called during startup or if the user launches the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

 protected:
  void DoInit() override;

 private:
  friend class base::RefCountedDeleteOnSequence<
      SnapshottingCommandStorageBackend>;
  friend class base::DeleteHelper<SnapshottingCommandStorageBackend>;

  FRIEND_TEST_ALL_PREFIXES(SnapshottingCommandStorageBackendTest,
                           ReadLegacySession);
  FRIEND_TEST_ALL_PREFIXES(SnapshottingCommandStorageBackendTest,
                           DeterminePreviousSessionEmpty);
  FRIEND_TEST_ALL_PREFIXES(SnapshottingCommandStorageBackendTest,
                           DeterminePreviousSessionSingle);
  FRIEND_TEST_ALL_PREFIXES(SnapshottingCommandStorageBackendTest,
                           DeterminePreviousSessionMultiple);
  FRIEND_TEST_ALL_PREFIXES(SnapshottingCommandStorageBackendTest,
                           DeterminePreviousSessionInvalid);

  ~SnapshottingCommandStorageBackend() override;

  // Represents data for a session.
  struct SessionInfo {
    base::FilePath path;
    base::Time timestamp;
  };

  // Gets data for the last session file.
  void DetermineLastSessionFile();

  // Attempt to delete all sessions besides the current and last. This is a
  // best effort operation.
  void DeleteLastSessionFiles();

  // Gets all sessions files.
  std::vector<SessionInfo> GetSessionFiles();

  // Timestamp when this session was started.
  base::Time timestamp_;

  // Directory files are relative to.
  const base::FilePath base_dir_;

  // Data for the last session. If unset, fallback to legacy session data.
  base::Optional<SessionInfo> last_session_info_;

  const SnapshottingCommandStorageManager::SessionType type_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SNAPSHOTTING_COMMAND_STORAGE_BACKEND_H_
