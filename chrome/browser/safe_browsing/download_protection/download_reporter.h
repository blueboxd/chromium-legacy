// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_

#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace safe_browsing {

// This class is responsible for observing download events and reporting them as
// appropriate.
class DownloadReporter
    : public download::DownloadItem::Observer,
      public download::SimpleDownloadManagerCoordinator::Observer,
      public content::NotificationObserver {
 public:
  DownloadReporter();
  ~DownloadReporter() override;

  // NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // SimpleDownloadManagerCoordinator::Observer implementation:
  void OnDownloadCreated(download::DownloadItem* download) override;

  // DownloadItem::Observer implementation:
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

 private:
  content::NotificationRegistrar profiles_registrar_;
  base::flat_map<download::DownloadItem*, download::DownloadDangerType>
      danger_types_;
  std::set<download::SimpleDownloadManagerCoordinator*> observed_coordinators_;
  std::set<download::DownloadItem*> observed_downloads_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REPORTER_H_
