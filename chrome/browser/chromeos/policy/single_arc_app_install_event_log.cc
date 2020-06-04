// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/single_arc_app_install_event_log.h"

#include "base/files/file.h"

namespace em = enterprise_management;

namespace policy {

SingleArcAppInstallEventLog::SingleArcAppInstallEventLog(
    const std::string& package)
    : SingleInstallEventLog(package) {}

SingleArcAppInstallEventLog::~SingleArcAppInstallEventLog() {}

bool SingleArcAppInstallEventLog::Load(
    base::File* file,
    std::unique_ptr<SingleArcAppInstallEventLog>* log) {
  log->reset();

  if (!file->IsValid()) {
    return false;
  }

  ssize_t size;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&size), sizeof(size)) !=
          sizeof(size) ||
      size > kMaxBufferSize) {
    return false;
  }

  std::unique_ptr<char[]> package_buffer = std::make_unique<char[]>(size);
  if (file->ReadAtCurrentPos(package_buffer.get(), size) != size) {
    return false;
  }

  *log = std::make_unique<SingleArcAppInstallEventLog>(
      std::string(package_buffer.get(), size));

  int64_t incomplete;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&incomplete),
                             sizeof(incomplete)) != sizeof(incomplete)) {
    return false;
  }
  (*log)->incomplete_ = incomplete;

  ssize_t entries;
  if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&entries),
                             sizeof(entries)) != sizeof(entries)) {
    return false;
  }

  for (ssize_t i = 0; i < entries; ++i) {
    if (file->ReadAtCurrentPos(reinterpret_cast<char*>(&size), sizeof(size)) !=
            sizeof(size) ||
        size > kMaxBufferSize) {
      (*log)->incomplete_ = true;
      return false;
    }

    if (size == 0) {
      // Zero-size entries are written if serialization of a log entry fails.
      // Skip these on read.
      (*log)->incomplete_ = true;
      continue;
    }

    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
    if (file->ReadAtCurrentPos(buffer.get(), size) != size) {
      (*log)->incomplete_ = true;
      return false;
    }

    em::AppInstallReportLogEvent event;
    if (event.ParseFromArray(buffer.get(), size)) {
      (*log)->Add(event);
    } else {
      (*log)->incomplete_ = true;
    }
  }

  return true;
}

void SingleArcAppInstallEventLog::Serialize(em::AppInstallReport* report) {
  report->Clear();
  report->set_package(id_);
  report->set_incomplete(incomplete_);
  for (const auto& event : events_) {
    em::AppInstallReportLogEvent* const log_event = report->add_logs();
    *log_event = event;
  }
  serialized_entries_ = events_.size();
}

}  // namespace policy
