// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/snapshotting_command_storage_backend.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/sessions/core/session_constants.h"

namespace sessions {
namespace {

base::FilePath::StringType TimestampToString(const base::Time time) {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#elif defined(OS_WIN)
  return base::NumberToString16(
      time.ToDeltaSinceWindowsEpoch().InMicroseconds());
#endif
}

base::FilePath::StringType GetSessionFilename(
    const SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath::StringType& timestamp_str) {
  if (type == SnapshottingCommandStorageManager::TAB_RESTORE)
    return base::JoinString({kTabSessionFileNamePrefix, timestamp_str},
                            FILE_PATH_LITERAL("_"));

  return base::JoinString({kSessionFileNamePrefix, timestamp_str},
                          FILE_PATH_LITERAL("_"));
}

base::FilePath GetLegacySessionPath(
    SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& base_path,
    bool current) {
  base::FilePath session_path = base_path;

  if (type == SnapshottingCommandStorageManager::TAB_RESTORE) {
    return session_path.Append(current ? kLegacyCurrentTabSessionFileName
                                       : kLegacyLastTabSessionFileName);
  }
  return session_path.Append(current ? kLegacyCurrentSessionFileName
                                     : kLegacyLastSessionFileName);
}

}  // namespace

SnapshottingCommandStorageBackend::SnapshottingCommandStorageBackend(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner,
    SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& path_to_dir)
    : CommandStorageBackend(
          std::move(owning_task_runner),
          FilePathFromTime(type, path_to_dir, base::Time::Now())),
      base_dir_(path_to_dir),
      type_(type) {
  // NOTE: this is invoked on the main thread, don't do file access here.

  // We need to retrieve the timestamp after initializing the superclass.
  if (!TimestampFromPath(path(), timestamp_))
    NOTREACHED();
}

// static
bool SnapshottingCommandStorageBackend::TimestampFromPath(
    const base::FilePath& path,
    base::Time& timestamp_result) {
  auto parts =
      base::SplitString(path.BaseName().value(), FILE_PATH_LITERAL("_"),
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 2u)
    return false;

  int64_t result = 0u;
  if (!base::StringToInt64(parts[1], &result))
    return false;

  timestamp_result = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(result));
  return true;
}

// static
base::FilePath SnapshottingCommandStorageBackend::FilePathFromTime(
    const SnapshottingCommandStorageManager::SessionType type,
    const base::FilePath& base_path,
    base::Time time) {
  return base_path.Append(kSessionsDirectory)
      .Append(GetSessionFilename(type, TimestampToString(time)));
}

std::vector<std::unique_ptr<SessionCommand>>
SnapshottingCommandStorageBackend::ReadLastSessionCommands() {
  InitIfNecessary();

  if (last_session_info_)
    return ReadCommandsFromFile(last_session_info_->path,
                                std::vector<uint8_t>());
  else
    return {};
}

void SnapshottingCommandStorageBackend::DeleteLastSession() {
  InitIfNecessary();
  if (last_session_info_) {
    base::DeleteFile(last_session_info_->path);
    last_session_info_.reset();
  }
}

void SnapshottingCommandStorageBackend::MoveCurrentSessionToLastSession() {
  InitIfNecessary();
  CloseFile();
  DeleteLastSession();

  // Move current session to last.
  if (base::PathExists(path()))
    last_session_info_ = SessionInfo{path(), timestamp_};
  else
    last_session_info_.reset();

  // Create new file, ensuring the timestamp is after the previous.
  // Due to clock changes, there might be an existing session with a later
  // time. This is especially true on Windows, which uses the local time as
  // the system clock.
  base::Time new_timestamp = base::Time::Now();
  if (!last_session_info_ || last_session_info_->timestamp < new_timestamp) {
    timestamp_ = new_timestamp;
  } else {
    timestamp_ =
        last_session_info_->timestamp + base::TimeDelta::FromMicroseconds(1);
  }
  SetPath(FilePathFromTime(type_, base_dir_, timestamp_));

  // Create and open the file for the current session.
  DCHECK(!base::PathExists(path()));
  TruncateFile();
}

void SnapshottingCommandStorageBackend::DoInit() {
  // Find the last session.
  DetermineLastSessionFile();
  if (last_session_info_) {
    // Check that the last session's timestamp is before the current file's.
    // This might not be true if the system clock has changed.
    if (last_session_info_->timestamp > timestamp_) {
      timestamp_ =
          last_session_info_->timestamp + base::TimeDelta::FromMicroseconds(1);
      SetPath(FilePathFromTime(type_, base_dir_, timestamp_));
    }
  }

  // Best effort delete all sessions except the current & last.
  DeleteLastSessionFiles();

  // Create and open the file for the current session.
  DCHECK(!base::PathExists(path()));
  TruncateFile();
}

void SnapshottingCommandStorageBackend::DetermineLastSessionFile() {
  last_session_info_.reset();

  // Determine the session with the most recent timestamp that
  // does not match the current session path.
  for (const SnapshottingCommandStorageBackend::SessionInfo& session :
       GetSessionFiles()) {
    if (session.path != path()) {
      if (!last_session_info_ ||
          session.timestamp > last_session_info_->timestamp)
        last_session_info_ = session;
    }
  }

  // If no last session was found, use the legacy session if present.
  // The legacy session is considered to have a timestamp of 0, before any
  // new session.
  if (!last_session_info_) {
    base::FilePath legacy_session =
        GetLegacySessionPath(type_, base_dir_, true);
    if (base::PathExists(legacy_session))
      last_session_info_ = SessionInfo{legacy_session, base::Time()};
  }
}

void SnapshottingCommandStorageBackend::DeleteLastSessionFiles() {
  // Delete session files whose paths do not match the current
  // or last session path.
  for (const SnapshottingCommandStorageBackend::SessionInfo& session :
       GetSessionFiles()) {
    if (session.path != path() &&
        (!last_session_info_ || session.path != last_session_info_->path)) {
      base::DeleteFile(session.path);
    }
  }

  // Delete legacy session files, unless they are being used.
  base::FilePath current_session_path =
      GetLegacySessionPath(type_, base_dir_, true);
  if (last_session_info_ && current_session_path != last_session_info_->path &&
      base::PathExists(current_session_path))
    base::DeleteFile(current_session_path);

  base::FilePath last_session_path =
      GetLegacySessionPath(type_, base_dir_, false);
  if (base::PathExists(last_session_path))
    base::DeleteFile(last_session_path);
}

std::vector<SnapshottingCommandStorageBackend::SessionInfo>
SnapshottingCommandStorageBackend::GetSessionFiles() {
  std::vector<SnapshottingCommandStorageBackend::SessionInfo> sessions;
  base::FileEnumerator file_enum(
      base_dir_.Append(kSessionsDirectory), false, base::FileEnumerator::FILES,
      GetSessionFilename(type_, FILE_PATH_LITERAL("*")));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::Time file_time;
    if (TimestampFromPath(name, file_time))
      sessions.push_back(SessionInfo{name, file_time});
  }
  return sessions;
}

SnapshottingCommandStorageBackend::~SnapshottingCommandStorageBackend() =
    default;

}  // namespace sessions
