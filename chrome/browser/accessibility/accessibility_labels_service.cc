// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_features.h"
#include "ui/accessibility/ax_action_data.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#endif

AccessibilityLabelsService::~AccessibilityLabelsService() {}

// static
void AccessibilityLabelsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsOptInAccepted, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void AccessibilityLabelsService::InitOffTheRecordPrefs(
    Profile* off_the_record_profile) {
  DCHECK(off_the_record_profile->IsOffTheRecord());
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsOptInAccepted, false);
}

void AccessibilityLabelsService::Init() {
  // Hidden behind a feature flag.
  if (!base::FeatureList::IsEnabled(features::kExperimentalAccessibilityLabels))
    return;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityImageLabelsEnabled,
      base::BindRepeating(
          &AccessibilityLabelsService::OnImageLabelsEnabledChanged,
          weak_factory_.GetWeakPtr()));

  // Log whether the feature is enabled after startup. This must be run on the
  // UI thread because it accesses prefs.
  content::BrowserAccessibilityState::GetInstance()
      ->AddUIThreadHistogramCallback(base::BindRepeating(
          &AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms,
          weak_factory_.GetWeakPtr()));
}

AccessibilityLabelsService::AccessibilityLabelsService(Profile* profile)
    : profile_(profile) {}

ui::AXMode AccessibilityLabelsService::GetAXMode() {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();

  // Hidden behind a feature flag.
  if (base::FeatureList::IsEnabled(
          features::kExperimentalAccessibilityLabels)) {
    bool enabled = profile_->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled);
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
  }

  return ax_mode;
}

void AccessibilityLabelsService::EnableLabelsServiceOnce() {
  if (!accessibility_state_utils::IsScreenReaderEnabled()) {
    return;
  }

  // TODO(crbug.com/905419): Implement for Android, which does not support
  // BrowserList::GetInstance.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return;
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  // Fire an AXAction on the active tab to enable this feature once only.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kAnnotatePageImages;
  for (content::RenderFrameHost* frame : web_contents->GetAllFrames()) {
    if (frame->IsRenderFrameLive())
      frame->AccessibilityPerformAction(action_data);
  }
#endif
}

void AccessibilityLabelsService::OnImageLabelsEnabledChanged() {
  // TODO(dmazzoni) Implement for Android, which doesn't support
  // AllTabContentses(). crbug.com/905419
#if !defined(OS_ANDROID)
  bool enabled = profile_->GetPrefs()->GetBoolean(
                     prefs::kAccessibilityImageLabelsEnabled) &&
                 accessibility_state_utils::IsScreenReaderEnabled();

  for (auto* web_contents : AllTabContentses()) {
    if (web_contents->GetBrowserContext() != profile_)
      continue;

    ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
    web_contents->SetAccessibilityMode(ax_mode);
  }
#endif
}

void AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms() {
  if (!profile_ || !profile_->GetPrefs())
    return;

  base::UmaHistogramBoolean("Accessibility.ImageLabels",
                            profile_->GetPrefs()->GetBoolean(
                                prefs::kAccessibilityImageLabelsEnabled));
}
