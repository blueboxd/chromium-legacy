// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "media/base/media_switches.h"

namespace captions {

CaptionController::CaptionController(Profile* profile) : profile_(profile) {}

CaptionController::~CaptionController() = default;

// static
void CaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterFilePathPref(prefs::kSODAPath, base::FilePath());
}

// static
void CaptionController::InitOffTheRecordPrefs(Profile* off_the_record_profile) {
  DCHECK(off_the_record_profile->IsOffTheRecord());
  off_the_record_profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 false);
  off_the_record_profile->GetPrefs()->SetFilePath(prefs::kSODAPath,
                                                  base::FilePath());
}

void CaptionController::Init() {
  // Hidden behind a feature flag.
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return;

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&CaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
}

void CaptionController::OnLiveCaptionEnabledChanged() {
  PrefService* profile_prefs = profile_->GetPrefs();
  bool enabled = profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled);
  if (enabled == enabled_)
    return;
  enabled_ = enabled;

  if (enabled_) {
    // Register SODA component and download speech model.
    component_updater::RegisterSODAComponent(
        g_browser_process->component_updater(), profile_prefs,
        base::BindOnce(&component_updater::SODAComponentInstallerPolicy::
                           UpdateSODAComponentOnDemand));

    // Create captions UI in each browser view.
    for (Browser* browser : *BrowserList::GetInstance()) {
      OnBrowserAdded(browser);
    }

    // Add observers to the BrowserList for new browser views being added.
    BrowserList::GetInstance()->AddObserver(this);
  } else {
    // TODO(evliu): Unregister SODA component.

    // Destroy caption bubble controllers.
    caption_bubble_controllers_.clear();

    // Remove observers.
    BrowserList::GetInstance()->RemoveObserver(this);
  }
}

void CaptionController::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  caption_bubble_controllers_[browser] =
      CaptionBubbleController::Create(browser);
}

void CaptionController::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  DCHECK(caption_bubble_controllers_.count(browser));
  caption_bubble_controllers_.erase(browser);
}

}  // namespace captions
