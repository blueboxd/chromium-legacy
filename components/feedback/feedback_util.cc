// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "components/feedback/feedback_report.h"
#include "third_party/zlib/google/zip.h"

namespace {

constexpr char kMultilineIndicatorString[] = "<multiline>\n";
constexpr char kMultilineStartString[] = "---------- START ----------\n";
constexpr char kMultilineEndString[] = "---------- END ----------\n\n";

}  // namespace

namespace feedback_util {

bool ZipString(const base::FilePath& filename,
               const std::string& data,
               std::string* compressed_logs) {
  base::ScopedTempDir temp_dir;
  base::FilePath zip_file;

  // Create a temporary directory, put the logs into a file in it. Create
  // another temporary file to receive the zip file in.
  if (!temp_dir.CreateUniqueTempDir())
    return false;
  if (base::WriteFile(temp_dir.GetPath().Append(filename), data.c_str(),
                      data.size()) == -1) {
    return false;
  }

  bool succeed = base::CreateTemporaryFile(&zip_file) &&
                 zip::Zip(temp_dir.GetPath(), zip_file, false) &&
                 base::ReadFileToString(zip_file, compressed_logs);

  base::DeleteFile(zip_file, false);

  return succeed;
}

std::string LogsToString(const FeedbackCommon::SystemLogsMap& sys_info) {
  std::string syslogs_string;
  for (const auto& iter : sys_info) {
    std::string key = iter.first;
    base::TrimString(key, "\n ", &key);

    if (key == feedback::FeedbackReport::kCrashReportIdsKey ||
        key == feedback::FeedbackReport::kAllCrashReportIdsKey) {
      // Avoid adding the crash IDs to the system_logs.txt file for privacy
      // reasons. They should just be part of the product specific data.
      continue;
    }

    std::string value = iter.second;
    base::TrimString(value, "\n ", &value);
    if (value.find("\n") != std::string::npos) {
      syslogs_string.append(key + "=" + kMultilineIndicatorString +
                            kMultilineStartString + value + "\n" +
                            kMultilineEndString);
    } else {
      syslogs_string.append(key + "=" + value + "\n");
    }
  }
  return syslogs_string;
}

}  // namespace feedback_util
