// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace {

// Default value for Finch parameter |kVersion|.
const int kInvalidVersion = -1;

}  // namespace

class AnnouncementNotificationServiceImpl
    : public AnnouncementNotificationService {
 public:
  AnnouncementNotificationServiceImpl(PrefService* pref_service,
                                      std::unique_ptr<Delegate> delegate)
      : pref_service_(pref_service),
        delegate_(std::move(delegate)),
        skip_first_run_(true),
        skip_display_(false),
        remote_version_(kInvalidVersion) {
    if (!IsFeatureEnabled())
      return;

    DCHECK(delegate_);

    // By default do nothing in first run.
    skip_first_run_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kSkipFirstRun, true);
    skip_display_ = base::GetFieldTrialParamByFeatureAsBool(
        kAnnouncementNotification, kSkipDisplay, false);
    remote_version_ = base::GetFieldTrialParamByFeatureAsInt(
        kAnnouncementNotification, kVersion, kInvalidVersion);
  }

  ~AnnouncementNotificationServiceImpl() override = default;

 private:
  // AnnouncementNotificationService implementation.
  void MaybeShowNotification() override {
    if (!IsFeatureEnabled())
      return;

    // No valid version Finch parameter.
    if (!IsVersionValid(remote_version_))
      return;

    int current_version = pref_service_->GetInteger(kCurrentVersionPrefName);
    pref_service_->SetInteger(kCurrentVersionPrefName, remote_version_);

    if (remote_version_ > current_version) {
      OnNewVersion();
    }
  }

  bool IsFeatureEnabled() const {
    return base::FeatureList::IsEnabled(kAnnouncementNotification);
  }

  bool IsVersionValid(int version) const { return version >= 0; }

  void OnFirstRun() {
    if (skip_first_run_)
      return;
    ShowNotification();
  }

  void OnNewVersion() {
    // Skip first run if needed.
    if (delegate_->IsFirstRun() && skip_first_run_)
      return;

    ShowNotification();
  }

  void ShowNotification() {
    if (skip_display_)
      return;

    delegate_->ShowNotification();
  }

  PrefService* pref_service_;
  std::unique_ptr<Delegate> delegate_;

  // Whether to skip first Chrome launch. Parsed from Finch.
  bool skip_first_run_;

  // Don't show notification if true. Parsed from Finch.
  bool skip_display_;

  // The new notification version. Parsed from Finch.
  int remote_version_;

  base::WeakPtrFactory<AnnouncementNotificationServiceImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AnnouncementNotificationServiceImpl);
};

const base::Feature kAnnouncementNotification{
    "AnnouncementNotificationService", base::FEATURE_DISABLED_BY_DEFAULT};

// static
void AnnouncementNotificationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterIntegerPref(kCurrentVersionPrefName, kInvalidVersion);
}

// static
AnnouncementNotificationService* AnnouncementNotificationService::Create(
    PrefService* pref_service,
    std::unique_ptr<Delegate> delegate) {
  return new AnnouncementNotificationServiceImpl(pref_service,
                                                 std::move(delegate));
}

AnnouncementNotificationService::AnnouncementNotificationService() = default;

AnnouncementNotificationService::~AnnouncementNotificationService() = default;
