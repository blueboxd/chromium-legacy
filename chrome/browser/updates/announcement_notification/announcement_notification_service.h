// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

// Whether to enable announcement notification system.
extern const base::Feature kAnnouncementNotification;

// The Finch parameter name for a boolean value that whether to show
// notification on first run.
constexpr char kSkipFirstRun[] = "skip_first_run";

// The Finch parameter to control whether to skip notification display.
constexpr char kSkipDisplay[] = "skip_display";

// The Finch parameter to define the latest version number of the notification.
constexpr char kVersion[] = "version";

// Preference name to persist the current version sent from Finch parameter.
constexpr char kCurrentVersionPrefName[] =
    "announcement_notification_service_current_version";

// Used to show a notification when the version defined in Finch parameter is
// higher than the last version saved in the preference service.
class AnnouncementNotificationService : public KeyedService {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Show notification.
    virtual void ShowNotification() = 0;

    // Is Chrome first time to run.
    virtual bool IsFirstRun() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static AnnouncementNotificationService* Create(
      PrefService* pref_service,
      std::unique_ptr<Delegate> delegate);

  AnnouncementNotificationService();
  ~AnnouncementNotificationService() override;

  // Show notification if needed based on a version number in Finch parameters
  // and the version cached in PrefService.
  virtual void MaybeShowNotification() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationService);
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_H_
