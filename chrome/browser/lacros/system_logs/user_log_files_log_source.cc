// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/system_logs/user_log_files_log_source.h"

#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kDefaultLogPath[] = "/home/chronos/user/lacros/lacros.log";
constexpr char kLogKey[] = "lacros_user_log";

// Maximum buffer size for user logs in bytes.
const int64_t kMaxLogSize = 1024 * 1024;
constexpr char kLogTruncated[] = "<earlier logs truncated>\n";
constexpr char kNotAvailable[] = "<not available>";

}  // namespace

UserLogFilesLogSource::UserLogFilesLogSource()
    : SystemLogsSource("UserLoggedFiles"),
      response_(std::make_unique<SystemLogsResponse>()) {}

UserLogFilesLogSource::~UserLogFilesLogSource() = default;

void UserLogFilesLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  auto* response_ptr = response.get();
  const base::FilePath log_file_path = base::FilePath(kDefaultLogPath);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&UserLogFilesLogSource::ReadFile,
                     weak_ptr_factory_.GetWeakPtr(), log_file_path, kLogKey,
                     response_ptr),
      base::BindOnce(std::move(callback), std::move(response)));

  callback_ = std::move(callback);
}

void UserLogFilesLogSource::ReadFile(const base::FilePath& log_file_path,
                                     const std::string& log_key,
                                     SystemLogsResponse* response) {
  std::string value;
  const bool read_success =
      feedback_util::ReadEndOfFile(log_file_path, kMaxLogSize, &value);

  if (read_success && value.length() == kMaxLogSize) {
    value.replace(0, strlen(kLogTruncated), kLogTruncated);

    LOG(WARNING) << "Large log file was likely truncated: " << log_file_path;
  }

  response->emplace(log_key, (read_success && !value.empty()) ? std::move(value)
                                                              : kNotAvailable);
}

}  // namespace system_logs
