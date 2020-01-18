// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_BACKEND_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_BACKEND_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/sessions_export.h"

namespace base {
class File;
}

namespace sessions {

// CommandStorageBackend is the backend used by CommandStorageManager. It writes
// SessionCommands to disk with the ability to read back at a later date.
// CommandStorageBackend does not interpret the commands in anyway, it simply
// reads/writes them.
class SESSIONS_EXPORT CommandStorageBackend
    : public base::RefCountedDeleteOnSequence<CommandStorageBackend> {
 public:
  using id_type = SessionCommand::id_type;
  using size_type = SessionCommand::size_type;
  using GetCommandsCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<SessionCommand>>)>;

  // Initial size of the buffer used in reading the file. This is exposed
  // for testing.
  static const int kFileReadBufferSize;

  // Creates a CommandStorageBackend. This method is invoked on the MAIN thread,
  // and does no IO. The real work is done from Init, which is invoked on
  // a background task runer.
  //
  // |path| is the path the file is written to.
  CommandStorageBackend(
      scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
      const base::FilePath& path);

  base::SequencedTaskRunner* owning_task_runner() {
    return base::RefCountedDeleteOnSequence<
        CommandStorageBackend>::owning_task_runner();
  }

  // Appends the specified commands to the current file. If |truncate| is true
  // the file is truncated.
  void AppendCommands(
      std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
      bool truncate);

  // Invoked from the service to read the commands that make up the last
  // session, invokes ReadLastSessionCommandsImpl to do the work.
  void ReadCurrentSessionCommands(
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
      GetCommandsCallback callback);

  bool inited() const { return inited_; }

 protected:
  virtual ~CommandStorageBackend();

  // Performs initialization on the background task run, calling DoInit() if
  // necessary.
  void InitIfNecessary();

  // Called the first time InitIfNecessary() is called.
  virtual void DoInit() {}

  const base::FilePath& path() const { return path_; }

  // Reads the commands from the current file.
  //
  // On success, the read commands are added to commands. It is up to the
  // caller to delete the commands.
  bool ReadCommandsFromFile(
      const base::FilePath& path,
      std::vector<std::unique_ptr<sessions::SessionCommand>>* commands);

  // Closes the file. The next time AppendCommands() is called the file will
  // implicitly be reopened.
  void CloseFile();

  // If current_session_file_ is open, it is truncated so that it is essentially
  // empty (only contains the header). If current_session_file_ isn't open, it
  // is is opened and the header is written to it. After this
  // current_session_file_ contains no commands.
  // NOTE: current_session_file_ may be null if the file couldn't be opened or
  // the header couldn't be written.
  void TruncateFile();

 private:
  friend class base::RefCountedDeleteOnSequence<CommandStorageBackend>;
  friend class base::DeleteHelper<CommandStorageBackend>;

  // Opens the current file and writes the header. On success a handle to
  // the file is returned.
  std::unique_ptr<base::File> OpenAndWriteHeader(const base::FilePath& path);

  // Appends the specified commands to the specified file.
  bool AppendCommandsToFile(
      base::File* file,
      const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands);

  // Path commands are saved to.
  const base::FilePath path_;

  // This may be null, created as necessary.
  std::unique_ptr<base::File> file_;

  // Whether DoInit() was called. DoInit() is called on the background task
  // runner.
  bool inited_ = false;

  DISALLOW_COPY_AND_ASSIGN(CommandStorageBackend);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_BACKEND_H_
