// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise_reporting/notification/extension_request_notification.h"
#include "chrome/browser/enterprise_reporting/report_generator.h"
#include "chrome/browser/enterprise_reporting/report_uploader.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/views/relaunch_notification/wall_clock_timer.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace enterprise_reporting {

// Schedules the next report and handles retry in case of error. It also cancels
// all pending uploads if the report policy is turned off.
class ReportScheduler : public ProfileManagerObserver {
 public:
  ReportScheduler(policy::CloudPolicyClient* client,
                  std::unique_ptr<ReportGenerator> report_generator);

  ~ReportScheduler() override;

  // Returns true if next report has been scheduled. The report will be
  // scheduled only if the previous report is uploaded successfully and the
  // reporting policy is still enabled.
  bool IsNextReportScheduledForTesting() const;

  void SetReportUploaderForTesting(std::unique_ptr<ReportUploader> uploader);

  void OnDMTokenUpdated();

 private:
  // Observes CloudReportingEnabled policy.
  void RegisterPrefObserver();

  // Handles kCloudReportingEnabled policy value change, including the first
  // policy value check during startup.
  void OnReportEnabledPrefChanged();

  // Stop |request_timer_| if it is existing.
  void StopRequestTimer();

  // Register |cloud_policy_client_| with dm token and client id for desktop
  // browser only. (Chrome OS doesn't need this step here.)
  bool SetupBrowserPolicyClientRegistration();

  // Schedules the first update request.
  void Start();

  // Generates a report and uploads it.
  void GenerateAndUploadReport();

  // Callback once report is generated.
  void OnReportGenerated(ReportGenerator::ReportRequests requests);

  // Callback once report upload request is finished.
  void OnReportUploaded(ReportUploader::ReportStatus status);

  // Tracks profiles that miss at least one report.
  void TrackStaleProfiles();

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // Policy value watcher
  PrefChangeRegistrar pref_change_registrar_;

  policy::CloudPolicyClient* cloud_policy_client_;

  WallClockTimer request_timer_;

  std::unique_ptr<ReportUploader> report_uploader_;

  std::unique_ptr<ReportGenerator> report_generator_;

  std::unique_ptr<base::flat_set<base::FilePath>> stale_profiles_;

  // Create an un-used notification instance so that the resources won't be
  // filtered out in the unit test. This will be removed once the whole
  // implementation is finished.
  ExtensionRequestNotification notification_;

  DISALLOW_COPY_AND_ASSIGN(ReportScheduler);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_H_
