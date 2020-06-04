// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SINGLE_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SINGLE_INSTALL_EVENT_LOG_H_

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <memory>
#include <string>
#include "base/files/file.h"

namespace policy {

// An event log for install process of single app. App refers to extension or
// ARC++ app. The log can be stored on disk and serialized for upload to a
// server. The log is internally held in a round-robin buffer. An |incomplete_|
// flag indicates whether any log entries were lost (e.g. entry too large or
// buffer wrapped around). Log entries are pruned and the flag is cleared after
// upload has completed. |T| specifies the event type.
template <typename T>
class SingleInstallEventLog {
 public:
  explicit SingleInstallEventLog(const std::string& id);
  ~SingleInstallEventLog();

  const std::string& id() const { return id_; }

  int size() const { return events_.size(); }

  bool empty() const { return events_.empty(); }

  // Add a log entry. If the buffer is full, the oldest entry is removed and
  // |incomplete_| is set.
  void Add(const T& event);

  // Stores the event log to |file|. Returns |true| if the log was written
  // successfully in a self-delimiting manner and the file may be used to store
  // further logs.
  bool Store(base::File* file) const;

  // Clears log entries that were previously serialized. Also clears
  // |incomplete_| if all log entries added since serialization are still
  // present in the log.
  void ClearSerialized();

  static const int kLogCapacity = 1024;
  static const int kMaxBufferSize = 65536;

 protected:
  // The app this event log pertains to.
  const std::string id_;

  // The buffer holding log entries.
  std::deque<T> events_;

  // Whether any log entries were lost (e.g. entry too large or buffer wrapped
  // around).
  bool incomplete_ = false;

  // The number of entries that were serialized and can be cleared from the log
  // after successful upload to the server, or -1 if none.
  int serialized_entries_ = -1;
};

// Implementation details below here.
template <typename T>
const int SingleInstallEventLog<T>::kLogCapacity;
template <typename T>
const int SingleInstallEventLog<T>::kMaxBufferSize;

template <typename T>
SingleInstallEventLog<T>::SingleInstallEventLog(const std::string& id)
    : id_(id) {}

template <typename T>
SingleInstallEventLog<T>::~SingleInstallEventLog() {}

template <typename T>
void SingleInstallEventLog<T>::Add(const T& event) {
  events_.push_back(event);
  if (events_.size() > kLogCapacity) {
    incomplete_ = true;
    if (serialized_entries_ > -1) {
      --serialized_entries_;
    }
    events_.pop_front();
  }
}

template <typename T>
bool SingleInstallEventLog<T>::Store(base::File* file) const {
  if (!file->IsValid()) {
    return false;
  }

  ssize_t size = id_.size();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&size),
                              sizeof(size)) != sizeof(size)) {
    return false;
  }

  if (file->WriteAtCurrentPos(id_.data(), size) != size) {
    return false;
  }

  const int64_t incomplete = incomplete_;
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&incomplete),
                              sizeof(incomplete)) != sizeof(incomplete)) {
    return false;
  }

  const ssize_t entries = events_.size();
  if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&entries),
                              sizeof(entries)) != sizeof(entries)) {
    return false;
  }

  for (const T& event : events_) {
    size = event.ByteSizeLong();
    std::unique_ptr<char[]> buffer;

    if (size > kMaxBufferSize) {
      // Log entry too large. Skip it.
      size = 0;
    } else {
      buffer = std::make_unique<char[]>(size);
      if (!event.SerializeToArray(buffer.get(), size)) {
        // Log entry serialization failed. Skip it.
        size = 0;
      }
    }

    if (file->WriteAtCurrentPos(reinterpret_cast<const char*>(&size),
                                sizeof(size)) != sizeof(size) ||
        (size && file->WriteAtCurrentPos(buffer.get(), size) != size)) {
      return false;
    }
  }

  return true;
}

template <typename T>
void SingleInstallEventLog<T>::ClearSerialized() {
  if (serialized_entries_ == -1) {
    return;
  }

  events_.erase(events_.begin(), events_.begin() + serialized_entries_);
  serialized_entries_ = -1;
  incomplete_ = false;
}

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SINGLE_INSTALL_EVENT_LOG_H_
