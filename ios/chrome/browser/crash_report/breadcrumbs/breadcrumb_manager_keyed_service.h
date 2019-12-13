// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_

#include <list>
#include <map>
#include <string>

#include "base/observer_list.h"
#import "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class BreadcrumbManagerObserver;

namespace web {
class BrowserState;
}

// Stores events logged with |AddEvent| in memory which can later be retrieved
// with |GetEvents|. Events will be silently dropped after a certain amount of
// time has passed unless no more recent events are available. The internal
// management of events aims to keep relevant events available while clearing
// stale data.
class BreadcrumbManagerKeyedService : public KeyedService {
 public:
  // Returns a list of the collected breadcrumb events which are still relevant
  // up to |event_count_limit|. Passing zero for |event_count_limit| signifies
  // no limit. Events returned will have a timestamp prepended to the original
  // |event| string representing when |AddEvent| was called.
  std::list<std::string> GetEvents(size_t event_count_limit);

  // Logs a breadcrumb event with message data |event|.
  // NOTE: |event| must not include newline characters as newlines are used by
  // BreadcrumbPersistentStore as a deliminator.
  void AddEvent(const std::string& event);

  // Adds and removes observers.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

  explicit BreadcrumbManagerKeyedService(web::BrowserState* browser_state);
  ~BreadcrumbManagerKeyedService() override;

 private:
  // Drops events which are considered stale. Note that stale events are not
  // guaranteed to be removed. Explicitly, stale events will be retained while
  // newer events are limited.
  void DropOldEvents();

  // A short string identifying the browser state used to initialize the
  // receiver. For example, "N" for "N"ormal browsing mode. This value is
  // prepended to events sent to |AddEvent| in order to differentiate the
  // BrowserState associated with each event.
  std::string browsing_mode_;

  // List of events, paired with the time which they were logged to minute
  // resolution. Newer events are at the end of the list.
  std::list<std::pair<base::Time, std::list<std::string>>> event_buckets_;

  base::ObserverList<BreadcrumbManagerObserver, /*check_empty=*/true>
      observers_;

  DISALLOW_COPY_AND_ASSIGN(BreadcrumbManagerKeyedService);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
