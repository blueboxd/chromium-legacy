// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log.h"

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"

namespace net {

TestNetLog::TestNetLog() {
  AddObserver(this, NetLogCaptureMode::kIncludeSensitive);
}

TestNetLog::~TestNetLog() {
  RemoveObserver(this);
}

void TestNetLog::GetEntries(TestNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = entry_list_;
}

void TestNetLog::GetEntriesForSource(NetLogSource source,
                                     TestNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  entry_list->clear();
  for (const auto& entry : entry_list_) {
    if (entry.source.id == source.id)
      entry_list->push_back(entry);
  }
}

size_t TestNetLog::GetSize() const {
  base::AutoLock lock(lock_);
  return entry_list_.size();
}

void TestNetLog::Clear() {
  base::AutoLock lock(lock_);
  entry_list_.clear();
}

void TestNetLog::OnAddEntry(const NetLogEntry& entry) {
  // Using Dictionaries instead of Values makes checking values a little
  // simpler.
  std::unique_ptr<base::DictionaryValue> param_dict =
      base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(entry.params.Clone()));

  // Only need to acquire the lock when accessing class variables.
  base::AutoLock lock(lock_);
  entry_list_.push_back(TestNetLogEntry(entry.type, base::TimeTicks::Now(),
                                        entry.source, entry.phase,
                                        std::move(param_dict)));
}

NetLog::ThreadSafeObserver* TestNetLog::GetObserver() {
  return this;
}

void TestNetLog::SetObserverCaptureMode(NetLogCaptureMode capture_mode) {
  RemoveObserver(this);
  AddObserver(this, capture_mode);
}

BoundTestNetLog::BoundTestNetLog()
    : net_log_(NetLogWithSource::Make(&test_net_log_, NetLogSourceType::NONE)) {
}

BoundTestNetLog::~BoundTestNetLog() = default;

void BoundTestNetLog::GetEntries(TestNetLogEntry::List* entry_list) const {
  test_net_log_.GetEntries(entry_list);
}

void BoundTestNetLog::GetEntriesForSource(
    NetLogSource source,
    TestNetLogEntry::List* entry_list) const {
  test_net_log_.GetEntriesForSource(source, entry_list);
}

size_t BoundTestNetLog::GetSize() const {
  return test_net_log_.GetSize();
}

void BoundTestNetLog::Clear() {
  test_net_log_.Clear();
}

void BoundTestNetLog::SetObserverCaptureMode(NetLogCaptureMode capture_mode) {
  test_net_log_.SetObserverCaptureMode(capture_mode);
}

}  // namespace net
