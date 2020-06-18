// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"

namespace {

const char kEventSeparator[] = "\n";

// Minimum time between breadcrumb writes to disk.
constexpr auto kMinDelayBetweenWrites = base::TimeDelta::FromMilliseconds(250);

// Writes |events| to |file_path| at |position|.
void DoInsertEventsIntoMemoryMappedFile(const base::FilePath& file_path,
                                        const size_t position,
                                        const std::string& events) {
  auto file = std::make_unique<base::MemoryMappedFile>();
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  const bool file_valid = file->Initialize(
      base::File(file_path, base::File::FLAG_OPEN_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE),
      region, base::MemoryMappedFile::READ_WRITE_EXTEND);

  if (file_valid) {
    char* data = reinterpret_cast<char*>(file->data());
    std::strcpy(&data[position], events.data());
  }
}

// Writes |events| to |file_path| overwriting any existing data.
void DoWriteEventsToFile(const base::FilePath& file_path,
                         const std::string& events) {
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  base::MemoryMappedFile file;
  const bool file_valid = file.Initialize(
      base::File(file_path, base::File::FLAG_CREATE_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE),
      region, base::MemoryMappedFile::READ_WRITE_EXTEND);

  if (file_valid) {
    char* data = reinterpret_cast<char*>(file.data());
    std::strcpy(data, events.data());
  }
}

void DoReplaceFile(const base::FilePath& from_path,
                   const base::FilePath& to_path) {
  base::ReplaceFile(from_path, to_path, nullptr);
}

// Returns breadcrumb events stored at |file_path|.
std::vector<std::string> DoGetStoredEvents(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    // File may not yet exist.
    return std::vector<std::string>();
  }

  size_t file_size = events_file.GetLength();
  if (file_size <= 0) {
    return std::vector<std::string>();
  }

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes) {
    file_size = kPersistedFilesizeInBytes;
  }

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data)) {
    return std::vector<std::string>();
  }
  std::string persisted_events(data.begin(), data.end());
  std::string all_events =
      persisted_events.substr(/*pos=*/0, strlen(persisted_events.c_str()));
  return base::SplitString(all_events, kEventSeparator, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageFilePath;
using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageTempFilePath;

BreadcrumbPersistentStorageKeyedService::
    BreadcrumbPersistentStorageKeyedService(web::BrowserState* browser_state)
    :  // Ensure first event will not be delayed by initializing with a time in
       // the past.
      last_written_time_(base::TimeTicks::Now() - kMinDelayBetweenWrites),
      browser_state_(browser_state),
      breadcrumbs_file_path_(
          GetBreadcrumbPersistentStorageFilePath(browser_state_)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      weak_factory_(this) {}

BreadcrumbPersistentStorageKeyedService::
    ~BreadcrumbPersistentStorageKeyedService() = default;

void BreadcrumbPersistentStorageKeyedService::GetStoredEvents(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetStoredEvents, breadcrumbs_file_path_),
      std::move(callback));
}

void BreadcrumbPersistentStorageKeyedService::StartStoringEvents() {
  RewriteAllExistingBreadcrumbs();

  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(browser_state_)
      ->AddObserver(this);
}

void BreadcrumbPersistentStorageKeyedService::RewriteAllExistingBreadcrumbs() {
  // Cancel writing out individual breadcrumbs as they are all being re-written.
  pending_breadcrumbs_.clear();
  write_timer_.Stop();

  last_written_time_ = base::TimeTicks::Now();

  current_mapped_file_position_ = 0;

  std::list<std::string> events =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(browser_state_)
          ->GetEvents(/*event_count_limit=*/0);

  std::vector<std::string> breadcrumbs;
  for (auto event_it = events.rbegin(); event_it != events.rend(); ++event_it) {
    // Reduce saved events to only fill the amount which would be included on
    // a crash log. This allows future events to be appended individually up to
    // |kPersistedFilesizeInBytes|, which is more efficient than writing out the
    const int event_with_seperator_size =
        event_it->size() + strlen(kEventSeparator);
    if (event_with_seperator_size + current_mapped_file_position_ >=
        kMaxBreadcrumbsDataLength) {
      break;
    }

    breadcrumbs.push_back(kEventSeparator);
    breadcrumbs.push_back(*event_it);
    current_mapped_file_position_ += event_with_seperator_size;
  }

  std::reverse(breadcrumbs.begin(), breadcrumbs.end());
  std::string breadcrumbs_string = base::JoinString(breadcrumbs, "");

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoWriteEventsToFile,
                     base::Passed(GetBreadcrumbPersistentStorageTempFilePath(
                         browser_state_)),
                     std::string(breadcrumbs_string)));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoReplaceFile,
                     base::Passed(GetBreadcrumbPersistentStorageTempFilePath(
                         browser_state_)),
                     breadcrumbs_file_path_));
}

void BreadcrumbPersistentStorageKeyedService::WritePendingBreadcrumbs() {
  if (pending_breadcrumbs_.empty()) {
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoInsertEventsIntoMemoryMappedFile,
                     breadcrumbs_file_path_, current_mapped_file_position_,
                     std::string(pending_breadcrumbs_)));

  current_mapped_file_position_ += pending_breadcrumbs_.size();
  last_written_time_ = base::TimeTicks::Now();

  pending_breadcrumbs_.clear();
}

void BreadcrumbPersistentStorageKeyedService::EventAdded(
    BreadcrumbManager* manager,
    const std::string& event) {
  // If the event doesn not fit within |kPersistedFilesizeInBytes|, rewrite the
  // file to trim old events.
  if ((current_mapped_file_position_ + pending_breadcrumbs_.size() +
       // Use >= here instead of > to allow space for \0 to terminate file.
       event.size()) >= kPersistedFilesizeInBytes) {
    RewriteAllExistingBreadcrumbs();
    return;
  }

  write_timer_.Stop();

  pending_breadcrumbs_ += event + kEventSeparator;

  const base::TimeDelta time_delta_since_last_write =
      base::TimeTicks::Now() - last_written_time_;
  // Delay writing the event to disk if an event was just written.
  if (time_delta_since_last_write < kMinDelayBetweenWrites) {
    write_timer_.Start(
        FROM_HERE, kMinDelayBetweenWrites - time_delta_since_last_write, this,
        &BreadcrumbPersistentStorageKeyedService::WritePendingBreadcrumbs);
  } else {
    WritePendingBreadcrumbs();
  }
}

void BreadcrumbPersistentStorageKeyedService::OldEventsRemoved(
    BreadcrumbManager* manager) {
  RewriteAllExistingBreadcrumbs();
}

void BreadcrumbPersistentStorageKeyedService::Shutdown() {
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(browser_state_)
      ->RemoveObserver(this);
}
