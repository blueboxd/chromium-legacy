// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"

class BreadcrumbManager;

namespace web {
class BrowserState;
}

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

  // Returns the stored breadcrumb events from disk.
  std::vector<std::string> GetStoredEvents();

  // Sets the |manager| to observe. Old stored breadcrumbs will be removed, even
  // if null is passed for |manager|. As such, this can be used to ensure stale
  // data does not take up disk space. If manager is non-null, events will be
  // written whenever |manager| receives an event.
  void ObserveBreadcrumbManager(BreadcrumbManager* manager);

 private:
  // Writes |observered_manager_|'s events to disk, overwriting any existing
  // persisted breadcrumbs. Removes all stored events if |observered_manager_|
  // is null.
  void WriteExistingBreadcrumbEvents();

  // Appends |event| to |persisted_events_file_|. |persisted_events_file_| must
  // already exist.
  void WriteBreadcrumbEvent(const std::string& event);

  // BreadcrumbManagerObserver
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override;

  // The breadcrumb manager currently being observed.
  BreadcrumbManager* observered_manager_ = nullptr;

  // The path to the breadcrumbs file.
  base::FilePath breadcrumbs_file_path_;

  // File handle for writing persisted breadcrumbs to |breadcrumbs_file_path_|.
  // Lazily created after |ObserveBreadcrumbManager| is called with a non-null
  // manager.
  std::unique_ptr<base::File> persisted_events_file_;

  DISALLOW_COPY_AND_ASSIGN(BreadcrumbPersistentStorageKeyedService);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_PERSISTENT_STORAGE_KEYED_SERVICE_H_
