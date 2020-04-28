// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kNotAvailable[] = "<not available>";
constexpr char kRoutesKeyName[] = "routes";
constexpr char kNetworkStatusKeyName[] = "network-status";
constexpr char kLogTruncated[] = "<earlier logs truncated>\n";

// List of user log files that Chrome reads directly as these logs are generated
// by Chrome itself.
constexpr struct UserLogs {
  // A string key used as a title for this log in feedback reports.
  const char* log_key;

  // The log file's path relative to the user's profile directory.
  const char* log_file_relative_path;
} kUserLogs[] = {
    {"chrome_user_log", "log/chrome"},
    {"chrome_user_log.PREVIOUS", "log/chrome.PREVIOUS"},
    {"libassistant_user_log", "log/libassistant.log"},
    {"login-times", "login-times"},
    {"logout-times", "logout-times"},
};

// Buffer size for user logs in bytes. Given that maximum feedback report size
// is ~7M and that majority of log files are under 1M, we set a per-file limit
// of 1MiB.
const int64_t kMaxLogSize = 1024 * 1024;

}  // namespace

bool ReadEndOfFile(const base::FilePath& path,
                   std::string* contents,
                   size_t max_size) {
  if (!contents) {
    LOG(ERROR) << "contents buffer is null.";
    return false;
  }

  if (path.ReferencesParent()) {
    LOG(ERROR) << "ReadEndOfFile can't be called on file paths with parent "
                  "references.";
    return false;
  }

  base::ScopedFILE fp(base::OpenFile(path, "r"));
  if (!fp) {
    PLOG(ERROR) << "Failed to open file " << path.value();
    return false;
  }

  std::unique_ptr<char[]> chunk(new char[max_size]);
  std::unique_ptr<char[]> last_chunk(new char[max_size]);
  chunk[0] = '\0';
  last_chunk[0] = '\0';

  size_t bytes_read = 0;

  // Since most logs are not seekable, read until the end keeping tracking of
  // last two chunks.
  while ((bytes_read = fread(chunk.get(), 1, max_size, fp.get())) == max_size) {
    last_chunk.swap(chunk);
    chunk[0] = '\0';
  }

  if (last_chunk[0] == '\0') {
    // File is smaller than max_size
    contents->assign(chunk.get(), bytes_read);
  } else if (bytes_read == 0) {
    // File is exactly max_size or a multiple of max_size
    contents->assign(last_chunk.get(), max_size);
  } else {
    // Number of bytes to keep from last_chunk
    size_t bytes_from_last = max_size - bytes_read;

    // Shift left last_chunk by size of chunk and fit it in the back of
    // last_chunk.
    memmove(last_chunk.get(), last_chunk.get() + bytes_read, bytes_from_last);
    memcpy(last_chunk.get() + bytes_from_last, chunk.get(), bytes_read);

    contents->assign(last_chunk.get(), max_size);
  }

  return true;
}

// Reads the contents of the user log files listed in |kUserLogs| and adds them
// to the |response| parameter.
void ReadUserLogFiles(const std::vector<base::FilePath>& profile_dirs,
                      SystemLogsResponse* response) {
  for (size_t i = 0; i < profile_dirs.size(); ++i) {
    std::string profile_prefix = "Profile[" + base::NumberToString(i) + "] ";
    for (const auto& log : kUserLogs) {
      std::string value;
      const bool read_success =
          ReadEndOfFile(profile_dirs[i].Append(log.log_file_relative_path),
                        &value, kMaxLogSize);

      if (read_success && value.length() == kMaxLogSize) {
        value.replace(0, strlen(kLogTruncated), kLogTruncated);

        LOG(WARNING) << "Large log file was likely truncated: "
                     << log.log_file_relative_path;
      }

      response->emplace(
          profile_prefix + log.log_key,
          (read_success && !value.empty()) ? std::move(value) : kNotAvailable);
    }
  }
}

DebugDaemonLogSource::DebugDaemonLogSource(bool scrub)
    : SystemLogsSource("DebugDemon"),
      response_(new SystemLogsResponse()),
      num_pending_requests_(0),
      scrub_(scrub) {}

DebugDaemonLogSource::~DebugDaemonLogSource() {}

void DebugDaemonLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  client->GetRoutes(true,   // Numeric
                    false,  // No IPv6
                    base::BindOnce(&DebugDaemonLogSource::OnGetRoutes,
                                   weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;

  client->GetNetworkStatus(base::BindOnce(&DebugDaemonLogSource::OnGetOneLog,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          kNetworkStatusKeyName));
  ++num_pending_requests_;

  if (scrub_) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    client->GetScrubbedBigLogs(
        cryptohome::CreateAccountIdentifierFromAccountId(
            user ? user->GetAccountId() : EmptyAccountId()),
        base::BindOnce(&DebugDaemonLogSource::OnGetLogs,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    client->GetAllLogs(base::BindOnce(&DebugDaemonLogSource::OnGetLogs,
                                      weak_ptr_factory_.GetWeakPtr()));
  }
  ++num_pending_requests_;
}

void DebugDaemonLogSource::OnGetRoutes(
    base::Optional<std::vector<std::string>> routes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  (*response_)[kRoutesKeyName] = routes.has_value()
                                     ? base::JoinString(routes.value(), "\n")
                                     : kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetOneLog(std::string key,
                                       base::Optional<std::string> status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  (*response_)[std::move(key)] = std::move(status).value_or(kNotAvailable);
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetLogs(bool /* succeeded */,
                                     const KeyValueMap& logs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We ignore 'succeeded' for this callback - we want to display as much of the
  // debug info as we can even if we failed partway through parsing, and if we
  // couldn't fetch any of it, none of the fields will even appear.
  response_->insert(logs.begin(), logs.end());
  RequestCompleted();
}

void DebugDaemonLogSource::GetLoggedInUsersLogFiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // List all logged-in users' profile directories.
  std::vector<base::FilePath> profile_dirs;
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (const auto* user : users) {
    if (user->username_hash().empty())
      continue;

    profile_dirs.emplace_back(
        chromeos::ProfileHelper::GetProfilePathByUserIdHash(
            user->username_hash()));
  }

  auto response = std::make_unique<SystemLogsResponse>();
  SystemLogsResponse* response_ptr = response.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadUserLogFiles, profile_dirs, response_ptr),
      base::BindOnce(&DebugDaemonLogSource::MergeUserLogFilesResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response)));
}

void DebugDaemonLogSource::MergeUserLogFilesResponse(
    std::unique_ptr<SystemLogsResponse> response) {
  for (auto& pair : *response)
    response_->emplace(pair.first, std::move(pair.second));

  auto response_to_return = std::make_unique<SystemLogsResponse>();
  std::swap(response_to_return, response_);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(response_to_return));
}

void DebugDaemonLogSource::RequestCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback_.is_null());

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  // When all other logs are collected, fetch the user logs, because any errors
  // fetching the other logs is reported in the user logs.
  GetLoggedInUsersLogFiles();
}

}  // namespace system_logs
