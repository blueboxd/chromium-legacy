// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"

namespace {

// Number of existing events to write to disk when beginning to observe a new
// BreadcrumbManager.
const int kPersistedExistingEventsCount = 10;

}  // namespace

using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageFilePath;

BreadcrumbPersistentStorageKeyedService::
    BreadcrumbPersistentStorageKeyedService(web::BrowserState* browser_state)
    : breadcrumbs_file_path_(
          GetBreadcrumbPersistentStorageFilePath(browser_state)) {}
BreadcrumbPersistentStorageKeyedService::
    ~BreadcrumbPersistentStorageKeyedService() = default;

std::vector<std::string>
BreadcrumbPersistentStorageKeyedService::GetStoredEvents() {
  base::File events_file(breadcrumbs_file_path_,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    // File may not yet exist.
    return std::vector<std::string>();
  }

  int64_t file_size = events_file.GetLength();
  if (file_size <= 0) {
    return std::vector<std::string>();
  }

  std::vector<uint8_t> data;
  // Subtract one to account for '\0' at end of file.
  data.resize(file_size - 1);
  if (!events_file.ReadAndCheck(/*offset=*/0, data)) {
    return std::vector<std::string>();
  }
  std::string persistent_events_string(data.begin(), data.end());
  return base::SplitString(persistent_events_string, "\n",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

void BreadcrumbPersistentStorageKeyedService::ObserveBreadcrumbManager(
    BreadcrumbManager* manager) {
  if (observered_manager_) {
    observered_manager_->RemoveObserver(this);
  }

  observered_manager_ = manager;
  WriteExistingBreadcrumbEvents();

  if (observered_manager_) {
    observered_manager_->AddObserver(this);
  }
}

void BreadcrumbPersistentStorageKeyedService::WriteExistingBreadcrumbEvents() {
  if (persisted_events_file_) {
    // File already exists, truncate to remove old events.
    persisted_events_file_->SetLength(0);
  } else if (observered_manager_) {
    persisted_events_file_ = std::make_unique<base::File>(
        breadcrumbs_file_path_,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  }

  if (!observered_manager_) {
    return;
  }

  for (auto& event :
       observered_manager_->GetEvents(kPersistedExistingEventsCount)) {
    WriteBreadcrumbEvent(event);
  }
  persisted_events_file_->Flush();
}

void BreadcrumbPersistentStorageKeyedService::WriteBreadcrumbEvent(
    const std::string& event) {
  DCHECK(persisted_events_file_);

  persisted_events_file_->WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span(event)));
  persisted_events_file_->WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span("\n")));
}

void BreadcrumbPersistentStorageKeyedService::EventAdded(
    BreadcrumbManager* manager,
    const std::string& event) {
  WriteBreadcrumbEvent(event);
  persisted_events_file_->Flush();
}
