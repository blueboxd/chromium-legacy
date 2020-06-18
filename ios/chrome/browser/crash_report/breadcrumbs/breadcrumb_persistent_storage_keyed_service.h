// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"
#include "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_constants.h"

namespace web {
class BrowserState;
}  // namespace web

// The filesize for the file at |breadcrumbs_file_path_|. The file will always
// be this constant size because it is accessed using a memory mapped file. The
// file is twice as large as |kMaxBreadcrumbsDataLength| which leaves room for
// appending breadcrumb events. Once the file is full of events, the contents
// will be reduced to kMaxBreadcrumbsDataLength.
constexpr size_t kPersistedFilesizeInBytes = kMaxBreadcrumbsDataLength * 2;

// Saves and retrieves breadcrumb events to and from disk.
class BreadcrumbPersistentStorageKeyedService
    : public BreadcrumbManagerObserver,
      public KeyedService {
 public:
  // Creates an instance to save and retrieve breadcrumb events from the file at
  // |file_path|. The file will be created if necessary.
  //  explicit BreadcrumbPersistentStorageKeyedService(const base::FilePath&
  //  file_path);
  explicit BreadcrumbPersistentStorageKeyedService(
      web::BrowserState* browser_state);
  ~BreadcrumbPersistentStorageKeyedService() override;

  // Returns the stored breadcrumb events from disk to |callback|. If called
  // before |StartStoringEvents|, these events (if any) will be from the prior
  // application session. After |StartStoringEvents| has been called, the
  // returned events will be from the current application session.
  void GetStoredEvents(
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Starts persisting breadcrumbs from the BreadcrumbManagerKeyedService
  // associated with |browser_state_|. This will overwrite any breadcrumbs which
  // may be stored from a previous application run.
  void StartStoringEvents();

 private:
  // Writes events from |observered_manager_| to |breadcrumbs_file_|,
  // overwriting any existing persisted breadcrumbs.
  void RewriteAllExistingBreadcrumbs();

  // Writes breadcrumbs stored in |pending_breadcrumbs_| to |breadcrumbs_file_|.
  void WritePendingBreadcrumbs();

  // BreadcrumbManagerObserver
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override;
  void OldEventsRemoved(BreadcrumbManager* manager) override;

  // KeyedService overrides
  void Shutdown() override;

  // Individual beadcrumbs which have not yet been written to disk.
  std::string pending_breadcrumbs_;

  // The last time a breadcrumb was written to |breadcrumbs_file_|. This
  // timestamp prevents breadcrumbs from being written to disk too often.
  base::TimeTicks last_written_time_;

  // A timer to delay writing to disk too often.
  base::OneShotTimer write_timer_;

  // The associated browser state.
  web::BrowserState* browser_state_ = nullptr;

  // The path to the file for storing persisted breadcrumbs.
  base::FilePath breadcrumbs_file_path_;

  // NOTE: Since this value represents the breadcrumbs written during this
  // session, it will remain 0 until |StartStoringEvents| is called.
  size_t current_mapped_file_position_ = 0;

  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BreadcrumbPersistentStorageKeyedService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BreadcrumbPersistentStorageKeyedService);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_
